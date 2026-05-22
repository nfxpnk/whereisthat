#include "FileScanner.h"
#include "../platform/PathHelpers.h"
#include "../platform/Win32Helpers.h"
#include <vector>
#include <Windows.h>

namespace wit::core {
bool FileScanner::ScanFolder(const std::wstring& rootPath, std::int64_t catalogId, wit::storage::Database& db, const ProgressCallback& onProgress){
    std::vector<std::wstring> stack{rootPath}; std::uint64_t count=0;
    db.BeginTransaction();
    while(!stack.empty()){
        auto dir=stack.back(); stack.pop_back();
        WIN32_FIND_DATAW fd{}; auto query = dir + L"\\*";
        HANDLE h = FindFirstFileW(query.c_str(), &fd); if (h==INVALID_HANDLE_VALUE) continue;
        do {
            if (wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0) continue;
            std::wstring full = dir + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){ if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) stack.push_back(full); continue; }
            wit::core::FileEntry e{}; e.catalogId=catalogId; e.name=fd.cFileName; e.parentPath=dir; e.extension=wit::platform::FileExtension(e.name);
            e.size=((uint64_t)fd.nFileSizeHigh<<32)|fd.nFileSizeLow; e.modifiedAt=wit::platform::FileTimeToIso8601(fd.ftLastWriteTime); e.attributes=fd.dwFileAttributes;
            db.InsertFile(e); ++count; if (onProgress && count % 250 == 0) onProgress({count, full});
        } while(FindNextFileW(h,&fd));
        FindClose(h);
    }
    db.UpdateCatalogItemCount(catalogId,count);
    db.Commit();
    if (onProgress) onProgress({count, rootPath});
    return true;
}
}
