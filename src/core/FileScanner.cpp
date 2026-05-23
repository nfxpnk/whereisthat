#include "FileScanner.h"
#include "../platform/PathHelpers.h"
#include "../platform/Win32Helpers.h"
#include <vector>
#include <Windows.h>

namespace wit::core {
namespace {
std::wstring JoinPath(const std::wstring& parent, const std::wstring& child) {
    if (parent.empty()) return child;
    wchar_t last = parent.back();
    return (last == L'\\' || last == L'/') ? parent + child : parent + L"\\" + child;
}

std::wstring WildcardFor(const std::wstring& path) {
    wchar_t last = path.empty() ? L'\0' : path.back();
    return (last == L'\\' || last == L'/') ? path + L"*" : path + L"\\*";
}

std::wstring DisplayNameForRoot(const std::wstring& rootPath) {
    if (rootPath.size() == 3 && rootPath[1] == L':' && (rootPath[2] == L'\\' || rootPath[2] == L'/')) return rootPath;
    auto end = rootPath.find_last_not_of(L"\\/");
    if (end == std::wstring::npos) return rootPath;
    auto pos = rootPath.find_last_of(L"\\/", end);
    return pos == std::wstring::npos ? rootPath.substr(0, end + 1) : rootPath.substr(pos + 1, end - pos);
}
}

bool FileScanner::ScanFolder(const std::wstring& rootPath, std::int64_t catalogId, wit::storage::Database& db,
    const ProgressCallback& onProgress, bool manageTransaction) {
    std::vector<std::wstring> stack{rootPath};
    std::uint64_t fileCount = 0;
    std::uint64_t folderCount = 0;
    if (manageTransaction && !db.BeginTransaction()) return false;
    const auto fail = [&db, manageTransaction]() {
        if (manageTransaction) db.Rollback();
        return false;
    };

    WIN32_FILE_ATTRIBUTE_DATA rootAttrs{};
    if (GetFileAttributesExW(rootPath.c_str(), GetFileExInfoStandard, &rootAttrs)) {
        wit::core::FileEntry root{};
        root.catalogId = catalogId;
        root.name = DisplayNameForRoot(rootPath);
        root.parentPath = L"";
        root.extension = L"";
        root.size = 0;
        root.modifiedAt = wit::platform::FileTimeToIso8601(rootAttrs.ftLastWriteTime);
        root.attributes = rootAttrs.dwFileAttributes;
        root.isDirectory = true;
        if (!db.InsertFile(root)) return fail();
        ++folderCount;
    }

    while (!stack.empty()) {
        auto directory = stack.back();
        stack.pop_back();
        WIN32_FIND_DATAW findData{};
        auto query = WildcardFor(directory);
        HANDLE findHandle = FindFirstFileW(query.c_str(), &findData);
        if (findHandle == INVALID_HANDLE_VALUE) continue;
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) continue;
            std::wstring fullPath = JoinPath(directory, findData.cFileName);
            wit::core::FileEntry entry{};
            entry.catalogId = catalogId;
            entry.name = findData.cFileName;
            entry.parentPath = directory;
            entry.extension = wit::platform::FileExtension(entry.name);
            entry.size = (static_cast<std::uint64_t>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
            entry.modifiedAt = wit::platform::FileTimeToIso8601(findData.ftLastWriteTime);
            entry.attributes = findData.dwFileAttributes;
            entry.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (entry.isDirectory) {
                entry.extension = L"";
                entry.size = 0;
                if (!db.InsertFile(entry)) {
                    FindClose(findHandle);
                    return fail();
                }
                ++folderCount;
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) stack.push_back(fullPath);
            } else {
                if (!db.InsertFile(entry)) {
                    FindClose(findHandle);
                    return fail();
                }
                ++fileCount;
            }
            auto total = fileCount + folderCount;
            if (onProgress && total % 250 == 0) onProgress({fileCount, folderCount, fullPath});
        } while (FindNextFileW(findHandle, &findData));
        FindClose(findHandle);
    }
    if (!db.UpdateCatalogItemCount(catalogId, fileCount + folderCount)) return fail();
    if (manageTransaction && !db.Commit()) return fail();
    if (onProgress) onProgress({fileCount, folderCount, rootPath});
    return true;
}
}
