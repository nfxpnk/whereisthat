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

bool FileScanner::ScanFolder(const std::wstring& rootPath, std::int64_t catalogId, wit::storage::Database& db, const ProgressCallback& onProgress){
    std::vector<std::wstring> stack{rootPath};
    std::uint64_t fileCount=0;
    std::uint64_t folderCount=0;
    if(!db.BeginTransaction()) return false;

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
        if(db.InsertFile(root)) ++folderCount;
    }

    while(!stack.empty()){
        auto dir=stack.back(); stack.pop_back();
        WIN32_FIND_DATAW fd{}; auto query = WildcardFor(dir);
        HANDLE h = FindFirstFileW(query.c_str(), &fd); if (h==INVALID_HANDLE_VALUE) continue;
        do {
            if (wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0) continue;
            std::wstring full = JoinPath(dir, fd.cFileName);
            wit::core::FileEntry e{}; e.catalogId=catalogId; e.name=fd.cFileName; e.parentPath=dir; e.extension=wit::platform::FileExtension(e.name);
            e.size=((uint64_t)fd.nFileSizeHigh<<32)|fd.nFileSizeLow; e.modifiedAt=wit::platform::FileTimeToIso8601(fd.ftLastWriteTime); e.attributes=fd.dwFileAttributes;
            e.isDirectory=(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0;
            if(e.isDirectory) {
                e.extension = L"";
                e.size = 0;
                if(db.InsertFile(e)) ++folderCount;
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) stack.push_back(full);
            } else {
                if(db.InsertFile(e)) ++fileCount;
            }
            auto total = fileCount + folderCount;
            if (onProgress && total % 250 == 0) onProgress({fileCount, folderCount, full});
        } while(FindNextFileW(h,&fd));
        FindClose(h);
    }
    db.UpdateCatalogItemCount(catalogId,fileCount + folderCount);
    db.Commit();
    if (onProgress) onProgress({fileCount, folderCount, rootPath});
    return true;
}
}
