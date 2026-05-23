#include "FileListView.h"
#include "../core/SizeFormat.h"
#include <CommCtrl.h>

namespace wit::ui {
void FileListView::SetCatalog(std::int64_t id, wit::storage::Database* database) {
    catalogId = id;
    db = database;
    total = db ? db->GetFileCountForCatalog(id) : 0;
    pageStart = -1;
    page.clear();
    ListView_SetItemCountEx(hwnd, total, LVSICF_NOINVALIDATEALL);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void FileListView::EnsurePage(int index) {
    if (!db) return;
    const int pageSize = 256;
    int desired = (index / pageSize) * pageSize;
    if (desired == pageStart) return;
    page = db->GetFilesPageForCatalog(catalogId, desired, pageSize);
    pageStart = desired;
}

std::wstring FileListView::TextFor(int row, int column) {
    EnsurePage(row);
    int index = row - pageStart;
    if (index < 0 || index >= static_cast<int>(page.size())) return L"";
    const auto& file = page[index];
    switch (column) {
    case 0:
        return file.name;
    case 1:
        return file.isDirectory ? L"Folder" : file.extension;
    case 2:
        return file.isDirectory ? L"" : wit::core::FormatSize(file.size);
    case 3:
        return file.parentPath;
    case 4:
        return file.modifiedAt;
    default:
        return L"";
    }
}
}
