#include "FileScanner.h"
#include "FolderEntry.h"
#include "../platform/PathHelpers.h"
#include "../platform/Win32Helpers.h"
#include <Windows.h>
#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace wit::core {
namespace {
// Enable while testing the progress dialog with a small scan source.
constexpr bool kEnableScanFileDelay = false;
constexpr std::int64_t kScanFileDelayMicroseconds = 50000;
constexpr std::uint64_t kProgressReportItemInterval = 250;

std::wstring JoinPath(const std::wstring& parent, const std::wstring& child) {
    if (parent.empty()) return child;
    wchar_t last = parent.back();
    return (last == L'\\' || last == L'/') ? parent + child : parent + L"\\" + child;
}

std::wstring WildcardFor(const std::wstring& path) {
    wchar_t last = path.empty() ? L'\0' : path.back();
    return (last == L'\\' || last == L'/') ? path + L"*" : path + L"\\*";
}

std::optional<std::wstring> FileCrc32(const std::wstring& path, std::stop_token stopToken, bool& cancelled) {
    const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    std::uint32_t crc = 0xFFFFFFFFu;
    std::vector<unsigned char> buffer(64 * 1024);
    DWORD count{};
    bool success = true;
    for (;;) {
        if (stopToken.stop_requested()) {
            cancelled = true;
            success = false;
            break;
        }
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
            success = false;
            break;
        }
        if (count == 0) break;
        for (DWORD index = 0; index < count; ++index) {
            crc ^= buffer[index];
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
            }
        }
    }
    CloseHandle(file);
    if (!success) return std::nullopt;
    std::wostringstream stream;
    stream << std::uppercase << std::hex << std::setfill(L'0') << std::setw(8) << (crc ^ 0xFFFFFFFFu);
    return stream.str();
}

FolderEntry FolderFromData(std::int64_t diskId, std::int64_t parentId, bool hasParent,
    const std::wstring& path, const std::wstring& name, const WIN32_FIND_DATAW& data) {
    FolderEntry folder;
    folder.diskId = diskId;
    folder.parentFolderId = parentId;
    folder.hasParent = hasParent;
    folder.path = path;
    folder.name = name;
    folder.createdAt = wit::platform::FileTimeToUnixSeconds(data.ftCreationTime);
    folder.modifiedAt = wit::platform::FileTimeToUnixSeconds(data.ftLastWriteTime);
    folder.accessedAt = wit::platform::FileTimeToUnixSeconds(data.ftLastAccessTime);
    folder.attributes = data.dwFileAttributes;
    return folder;
}
}

bool FileScanner::ScanFolder(const std::wstring& rootPath, std::int64_t diskId, wit::storage::Database& db,
    const ProgressCallback& onProgress, Result& result, bool calculateCrc, bool manageTransaction,
    std::stop_token stopToken) {
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t fileCount = 0;
    std::uint64_t folderCount = 0;
    if (manageTransaction && !db.BeginTransaction()) return false;
    const auto fail = [&db, manageTransaction]() {
        if (manageTransaction) db.Rollback();
        return false;
    };
    const auto cancelled = [&stopToken, &fail]() {
        if (!stopToken.stop_requested()) return false;
        fail();
        return true;
    };

    if (cancelled()) return false;
    WIN32_FILE_ATTRIBUTE_DATA rootAttrs{};
    if (!GetFileAttributesExW(rootPath.c_str(), GetFileExInfoStandard, &rootAttrs)) return fail();
    WIN32_FIND_DATAW rootData{};
    rootData.dwFileAttributes = rootAttrs.dwFileAttributes;
    rootData.ftCreationTime = rootAttrs.ftCreationTime;
    rootData.ftLastWriteTime = rootAttrs.ftLastWriteTime;
    rootData.ftLastAccessTime = rootAttrs.ftLastAccessTime;
    auto root = FolderFromData(diskId, 0, false, rootPath, wit::platform::DisplayNameForPath(rootPath), rootData);
    const auto rootId = db.InsertFolder(root);
    if (rootId == 0) return fail();
    ++folderCount;
    if (onProgress) onProgress({fileCount, folderCount, rootPath});

    struct FolderFrame {
        std::wstring path;
        std::int64_t id{};
        std::vector<std::pair<std::wstring, std::int64_t>> children;
        std::size_t nextChild{};
        std::uint64_t contentSize{};
        bool enumerated{};
    };
    std::vector<FolderFrame> stack{{rootPath, rootId}};
    while (!stack.empty()) {
        if (cancelled()) return false;
        auto& frame = stack.back();
        if (!frame.enumerated) {
            frame.enumerated = true;
            WIN32_FIND_DATAW findData{};
            const auto query = WildcardFor(frame.path);
            HANDLE findHandle = FindFirstFileW(query.c_str(), &findData);
            if (findHandle != INVALID_HANDLE_VALUE) {
                do {
                    if (stopToken.stop_requested()) {
                        FindClose(findHandle);
                        return fail();
                    }
                    if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                        continue;
                    }
                    const std::wstring fullPath = JoinPath(frame.path, findData.cFileName);
                    const bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    if (isDirectory) {
                        auto folder = FolderFromData(diskId, frame.id, true, fullPath, findData.cFileName, findData);
                        const auto childId = db.InsertFolder(folder);
                        if (childId == 0) {
                            FindClose(findHandle);
                            return fail();
                        }
                        ++folderCount;
                        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                            frame.children.emplace_back(fullPath, childId);
                        }
                    } else {
                        if constexpr (kEnableScanFileDelay) {
                            if (fileCount != 0) {
                                std::this_thread::sleep_for(
                                    std::chrono::microseconds{kScanFileDelayMicroseconds});
                            }
                        }
                        wit::core::FileEntry entry{};
                        entry.catalogId = diskId;
                        entry.folderId = frame.id;
                        entry.name = findData.cFileName;
                        entry.extension = wit::platform::FileExtension(entry.name);
                        entry.size = (static_cast<std::uint64_t>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
                        entry.createdAt = wit::platform::FileTimeToUnixSeconds(findData.ftCreationTime);
                        entry.modifiedAtValue = wit::platform::FileTimeToUnixSeconds(findData.ftLastWriteTime);
                        entry.accessedAt = wit::platform::FileTimeToUnixSeconds(findData.ftLastAccessTime);
                        entry.attributes = findData.dwFileAttributes;
                        bool crcCancelled{};
                        if (calculateCrc) entry.crc = FileCrc32(fullPath, stopToken, crcCancelled);
                        if (crcCancelled || stopToken.stop_requested()) {
                            FindClose(findHandle);
                            return fail();
                        }
                        if (!db.InsertFile(entry)) {
                            FindClose(findHandle);
                            return fail();
                        }
                        frame.contentSize += entry.size;
                        ++fileCount;
                    }
                    const auto total = fileCount + folderCount;
                    if (onProgress && (kEnableScanFileDelay || total % kProgressReportItemInterval == 0)) {
                        onProgress({fileCount, folderCount, fullPath});
                    }
                } while (FindNextFileW(findHandle, &findData));
                FindClose(findHandle);
            }
        }
        if (frame.nextChild < frame.children.size()) {
            const auto child = frame.children[frame.nextChild++];
            stack.push_back({child.first, child.second});
            continue;
        }
        const auto completedSize = frame.contentSize;
        const auto completedId = frame.id;
        if (!db.UpdateFolderContentSize(completedId, completedSize)) {
            return fail();
        }
        stack.pop_back();
        if (!stack.empty()) {
            stack.back().contentSize += completedSize;
        }
    }
    if (cancelled()) return false;
    if (manageTransaction && !db.Commit()) return fail();
    result.totalFiles = fileCount;
    result.totalFolders = folderCount;
    result.elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (onProgress) onProgress({fileCount, folderCount, rootPath});
    return true;
}
}
