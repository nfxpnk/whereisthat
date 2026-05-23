#include "FileListView.h"
#include "../core/SizeFormat.h"
#include <CommCtrl.h>

namespace wit::ui {
void FileListView::SetLocation(const wit::core::BrowserLocation& newLocation, wit::storage::Database* database) {
    location = newLocation;
    db = database;
    total = db ? db->GetBrowserItemCount(location) : 0;
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
    page = db->GetBrowserItemsPage(location, desired, pageSize);
    pageStart = desired;
}

const wit::core::FileEntry* FileListView::EntryAt(int row) {
    EnsurePage(row);
    const int index = row - pageStart;
    return index >= 0 && index < static_cast<int>(page.size()) ? &page[index] : nullptr;
}

std::wstring FileListView::TextFor(int row, int column) {
    const auto* entry = EntryAt(row);
    if (!entry) return L"";
    const auto& file = *entry;
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
