#include "FileScanner.h"
#include "FolderEntry.h"
#include "../platform/PathHelpers.h"
#include "../platform/Win32Helpers.h"
#include <Windows.h>
#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace wit::core {
namespace {
constexpr bool kEnableScanFileDelay = false;
constexpr std::int64_t kScanFileDelayMicroseconds = 50000;
constexpr std::uint64_t kProgressReportItemInterval = 250;

std::wstring JoinPath(const std::wstring& parent, const std::wstring& child) {
    if (parent.empty()) return child;
    const auto last = parent.back();
    return (last == L'\\' || last == L'/') ? parent + child : parent + L"\\" + child;
}

std::wstring WildcardFor(const std::wstring& path) {
    const auto last = path.empty() ? L'\0' : path.back();
    return (last == L'\\' || last == L'/') ? path + L"*" : path + L"\\*";
}

void UpdateCrc(std::uint32_t& crc, const unsigned char* buffer, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        crc ^= buffer[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
}

std::wstring FormatCrc(std::uint32_t crc) {
    std::wostringstream stream;
    stream << std::uppercase << std::hex << std::setfill(L'0') << std::setw(8) << (crc ^ 0xFFFFFFFFu);
    return stream.str();
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
        UpdateCrc(crc, buffer.data(), count);
    }
    CloseHandle(file);
    return success ? std::optional<std::wstring>{FormatCrc(crc)} : std::nullopt;
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

bool LooksLikeArchiveName(const std::wstring& name) {
    auto extension = wit::platform::FileExtension(name);
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
    return extension == L"zip" || extension == L"7z" || extension == L"rar" || extension == L"tar" ||
        extension == L"tgz" || extension == L"gz" || extension == L"bz2" || extension == L"xz" ||
        extension == L"cab" || extension == L"arj" || extension == L"lha" || extension == L"iso";
}

bool NormalizeArchivePath(const std::wstring& source, std::vector<std::wstring>& parts) {
    parts.clear();
    if (source.empty() || source.front() == L'\\' || source.front() == L'/' ||
        (source.size() >= 2 && source[1] == L':')) {
        return false;
    }
    std::wstring part;
    const auto flush = [&parts, &part]() {
        if (part.empty() || part == L".") {
            part.clear();
            return true;
        }
        if (part == L".." || part.find(L':') != std::wstring::npos) return false;
        parts.push_back(part);
        part.clear();
        return true;
    };
    for (const auto character : source) {
        if (character == L'\\' || character == L'/') {
            if (!flush()) return false;
        } else {
            part.push_back(character);
        }
    }
    return flush() && !parts.empty();
}

struct ArchiveMember {
    std::vector<std::wstring> parts;
    bool isDirectory{};
    std::uint64_t size{};
    std::int64_t modifiedAt{};
    std::optional<std::wstring> crc;
};

struct ParsedArchive {
    std::vector<ArchiveMember> members;
    bool recognized{};
};

enum class ArchiveReadResult {
    Readable,
    NotReadable,
    Cancelled
};

std::wstring ArchiveError(struct archive* reader) {
    const auto* error = archive_error_string(reader);
    return error ? wit::platform::ToUtf16(error) : L"archive data could not be read";
}

ArchiveReadResult ReadArchive(const std::wstring& path, bool calculateCrc, std::stop_token stopToken,
    ParsedArchive& parsed, std::wstring& failure) {
    auto* reader = archive_read_new();
    if (!reader) {
        failure = L"libarchive reader allocation failed";
        return ArchiveReadResult::NotReadable;
    }
    archive_read_support_filter_all(reader);
    archive_read_support_format_7zip(reader);
    archive_read_support_format_ar(reader);
    archive_read_support_format_cab(reader);
    archive_read_support_format_cpio(reader);
    archive_read_support_format_iso9660(reader);
    archive_read_support_format_lha(reader);
    archive_read_support_format_rar(reader);
    archive_read_support_format_rar5(reader);
    archive_read_support_format_tar(reader);
    archive_read_support_format_xar(reader);
    archive_read_support_format_zip(reader);
    if (archive_read_open_filename_w(reader, path.c_str(), 64 * 1024) != ARCHIVE_OK) {
        failure = ArchiveError(reader);
        archive_read_free(reader);
        return ArchiveReadResult::NotReadable;
    }
    bool success = true;
    for (;;) {
        if (stopToken.stop_requested()) {
            archive_read_free(reader);
            return ArchiveReadResult::Cancelled;
        }
        struct archive_entry* entry{};
        const auto next = archive_read_next_header(reader, &entry);
        if (next == ARCHIVE_EOF) break;
        if (next != ARCHIVE_OK) {
            failure = ArchiveError(reader);
            success = false;
            break;
        }
        parsed.recognized = true;
        if (archive_entry_is_encrypted(entry) > 0) {
            failure = L"encrypted archive entry cannot be read";
            success = false;
            break;
        }
        const auto* entryPath = archive_entry_pathname_w(entry);
        ArchiveMember member;
        if (!entryPath || !NormalizeArchivePath(entryPath, member.parts)) {
            failure = L"archive contains an unsafe or invalid member path";
            success = false;
            break;
        }
        member.isDirectory = archive_entry_filetype(entry) == AE_IFDIR;
        member.modifiedAt = archive_entry_mtime_is_set(entry) ? archive_entry_mtime(entry) : 0;
        if (member.isDirectory) {
            if (archive_read_data_skip(reader) != ARCHIVE_OK) {
                failure = ArchiveError(reader);
                success = false;
                break;
            }
        } else {
            std::uint32_t crc = 0xFFFFFFFFu;
            std::vector<unsigned char> buffer(64 * 1024);
            for (;;) {
                if (stopToken.stop_requested()) {
                    archive_read_free(reader);
                    return ArchiveReadResult::Cancelled;
                }
                const auto read = archive_read_data(reader, buffer.data(), buffer.size());
                if (read == 0) break;
                if (read < 0) {
                    failure = ArchiveError(reader);
                    success = false;
                    break;
                }
                member.size += static_cast<std::uint64_t>(read);
                if (calculateCrc) UpdateCrc(crc, buffer.data(), static_cast<std::size_t>(read));
            }
            if (!success) break;
            if (calculateCrc) member.crc = FormatCrc(crc);
        }
        parsed.members.push_back(std::move(member));
    }
    archive_read_free(reader);
    return success ? ArchiveReadResult::Readable : ArchiveReadResult::NotReadable;
}

struct CaseInsensitiveLess {
    bool operator()(const std::wstring& left, const std::wstring& right) const {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    }
};

struct StoredArchive {
    std::uint64_t contentSize{};
    std::uint64_t folders{};
    std::uint64_t files{};
};

bool StoreArchive(const ParsedArchive& parsed, const std::wstring& physicalPath, const WIN32_FIND_DATAW& data,
    std::int64_t diskId, std::int64_t parentId, wit::storage::Database& db, StoredArchive& stored) {
    auto archiveFolder = FolderFromData(diskId, parentId, true, physicalPath, data.cFileName, data);
    archiveFolder.entryType = FolderEntryType::Archive;
    const auto archiveId = db.InsertFolder(archiveFolder);
    if (archiveId == 0) return false;

    std::map<std::wstring, std::int64_t, CaseInsensitiveLess> folderIds{{L"", archiveId}};
    std::map<std::wstring, std::uint64_t, CaseInsensitiveLess> folderSizes{{L"", 0}};
    auto ensureFolders = [&](const std::vector<std::wstring>& parts, std::size_t count) {
        std::wstring relative;
        std::int64_t currentId = archiveId;
        for (std::size_t index = 0; index < count; ++index) {
            relative = JoinPath(relative, parts[index]);
            const auto found = folderIds.find(relative);
            if (found != folderIds.end()) {
                currentId = found->second;
                continue;
            }
            FolderEntry folder = archiveFolder;
            folder.parentFolderId = currentId;
            folder.path = JoinPath(physicalPath, relative);
            folder.name = parts[index];
            folder.entryType = FolderEntryType::Directory;
            folder.contentSize = 0;
            folder.attributes = 0;
            currentId = db.InsertFolder(folder);
            if (currentId == 0) return std::int64_t{0};
            folderIds.emplace(relative, currentId);
            folderSizes.emplace(relative, 0);
            ++stored.folders;
        }
        return currentId;
    };

    for (const auto& member : parsed.members) {
        const auto parentCount = member.isDirectory ? member.parts.size() : member.parts.size() - 1;
        const auto folderId = ensureFolders(member.parts, parentCount);
        if (folderId == 0) return false;
        if (member.isDirectory) continue;
        FileEntry file;
        file.catalogId = diskId;
        file.folderId = folderId;
        file.name = member.parts.back();
        file.extension = wit::platform::FileExtension(file.name);
        file.size = member.size;
        file.createdAt = archiveFolder.createdAt;
        file.modifiedAtValue = member.modifiedAt != 0 ? member.modifiedAt : archiveFolder.modifiedAt;
        file.accessedAt = archiveFolder.accessedAt;
        file.crc = member.crc;
        if (!db.InsertFile(file)) return false;
        ++stored.files;
        stored.contentSize += member.size;
        folderSizes[L""] += member.size;
        std::wstring relative;
        for (std::size_t index = 0; index < parentCount; ++index) {
            relative = JoinPath(relative, member.parts[index]);
            folderSizes[relative] += member.size;
        }
    }
    for (const auto& item : folderIds) {
        if (!db.UpdateFolderContentSize(item.second, folderSizes[item.first])) return false;
    }
    return true;
}

void ReportArchiveFailure(const FileScanner::DiagnosticCallback& onDiagnostic, const std::wstring& path,
    const std::wstring& failure) {
    const auto text = L"Could not read archive '" + path + L"': " + failure;
    if (onDiagnostic) {
        onDiagnostic(text);
    } else {
        OutputDebugStringW((text + L"\n").c_str());
    }
}
}

bool FileScanner::ScanFolder(const std::wstring& rootPath, std::int64_t diskId, wit::storage::Database& db,
    const ProgressCallback& onProgress, Result& result, bool calculateCrc, bool manageTransaction,
    std::stop_token stopToken, bool browseArchives, const DiagnosticCallback& onDiagnostic) {
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t fileCount = 0;
    std::uint64_t folderCount = 0;
    std::uint64_t archiveCount = 0;
    std::uint64_t archiveFileCount = 0;
    std::uint64_t archiveFolderCount = 0;
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
                        bool storedAsArchive = false;
                        if (browseArchives) {
                            ParsedArchive parsed;
                            std::wstring failure;
                            const auto read = ReadArchive(fullPath, calculateCrc, stopToken, parsed, failure);
                            if (read == ArchiveReadResult::Cancelled) {
                                FindClose(findHandle);
                                return fail();
                            }
                            if (read == ArchiveReadResult::Readable) {
                                StoredArchive stored;
                                if (!StoreArchive(parsed, fullPath, findData, diskId, frame.id, db, stored)) {
                                    FindClose(findHandle);
                                    return fail();
                                }
                                ++archiveCount;
                                archiveFileCount += stored.files;
                                archiveFolderCount += stored.folders;
                                fileCount += stored.files;
                                folderCount += 1 + stored.folders;
                                frame.contentSize += stored.contentSize;
                                storedAsArchive = true;
                            } else if (parsed.recognized || LooksLikeArchiveName(findData.cFileName)) {
                                ReportArchiveFailure(onDiagnostic, fullPath, failure);
                            }
                        }
                        if (!storedAsArchive) {
                            FileEntry entry{};
                            entry.catalogId = diskId;
                            entry.folderId = frame.id;
                            entry.name = findData.cFileName;
                            entry.extension = wit::platform::FileExtension(entry.name);
                            entry.size = (static_cast<std::uint64_t>(findData.nFileSizeHigh) << 32) |
                                findData.nFileSizeLow;
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
        if (!db.UpdateFolderContentSize(completedId, completedSize)) return fail();
        stack.pop_back();
        if (!stack.empty()) stack.back().contentSize += completedSize;
    }
    if (cancelled()) return false;
    if (manageTransaction && !db.Commit()) return fail();
    result.totalFiles = fileCount;
    result.totalFolders = folderCount;
    result.scannedArchives = archiveCount;
    result.archiveFiles = archiveFileCount;
    result.archiveFolders = archiveFolderCount;
    result.elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (onProgress) onProgress({fileCount, folderCount, rootPath});
    return true;
}
}
