#include "FileListView.h"
#include "../core/SizeFormat.h"
#include "../platform/Win32Helpers.h"
#include <CommCtrl.h>

namespace wit::ui {
namespace {
const wchar_t* DiskTypeLabel(wit::core::DiskType type) {
    switch (type) {
    case wit::core::DiskType::CD: return L"CD";
    case wit::core::DiskType::DVD: return L"DVD";
    case wit::core::DiskType::BluRay: return L"BluRay";
    case wit::core::DiskType::HardDisk: return L"HardDisk";
    case wit::core::DiskType::SolidStateDisk: return L"SolidStateDisk";
    case wit::core::DiskType::RemovableUSB: return L"RemovableUSB";
    case wit::core::DiskType::VirtualDisk: return L"VirtualDisk";
    default: return L"Other";
    }
}

void InsertColumn(HWND hwnd, int index, const wchar_t* name, int width, int format = LVCFMT_LEFT) {
    LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH | LVCF_FMT};
    column.fmt = format;
    column.cx = width;
    column.pszText = const_cast<LPWSTR>(name);
    ListView_InsertColumn(hwnd, index, &column);
}
}

void FileListView::ConfigureColumns() {
    while (ListView_DeleteColumn(hwnd, 0)) {}
    if (ShowsDisks()) {
        InsertColumn(hwnd, 0, L"Disk Name", 180);
        InsertColumn(hwnd, 1, L"Media Type", 100);
        InsertColumn(hwnd, 2, L"Capacity", 100, LVCFMT_RIGHT);
        InsertColumn(hwnd, 3, L"Free Space", 100, LVCFMT_RIGHT);
        InsertColumn(hwnd, 4, L"Last Updated", 165);
        InsertColumn(hwnd, 5, L"Disk #", 70, LVCFMT_RIGHT);
        InsertColumn(hwnd, 6, L"Description", 180);
        InsertColumn(hwnd, 7, L"Category", 120);
        InsertColumn(hwnd, 8, L"Disk Location", 240);
        return;
    }
    InsertColumn(hwnd, 0, L"Name", 200);
    InsertColumn(hwnd, 1, L"Type", 80);
    InsertColumn(hwnd, 2, L"Size", 100, LVCFMT_RIGHT);
    InsertColumn(hwnd, 3, L"Path", 320);
    InsertColumn(hwnd, 4, L"Modified", 180);
}

void FileListView::SetLocation(const wit::core::BrowserLocation& newLocation, wit::storage::Database* database) {
    location = newLocation;
    db = database;
    total = 0;
    pageStart = -1;
    page.clear();
    diskPage.clear();
    ListView_SetItemCountEx(hwnd, 0, LVSICF_NOINVALIDATEALL);
    ConfigureColumns();
    total = db ? (ShowsDisks() ? db->GetDiskCount() : db->GetBrowserItemCount(location)) : 0;
    ListView_SetItemCountEx(hwnd, total, LVSICF_NOINVALIDATEALL);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void FileListView::EnsurePage(int index) {
    if (!db) return;
    const int pageSize = 256;
    int desired = (index / pageSize) * pageSize;
    if (desired == pageStart) return;
    if (ShowsDisks()) {
        diskPage = db->GetDisksPage(desired, pageSize);
        page.clear();
    } else {
        page = db->GetBrowserItemsPage(location, desired, pageSize);
        diskPage.clear();
    }
    pageStart = desired;
}

const wit::core::FileEntry* FileListView::EntryAt(int row) {
    if (ShowsDisks()) return nullptr;
    EnsurePage(row);
    const int index = row - pageStart;
    return index >= 0 && index < static_cast<int>(page.size()) ? &page[index] : nullptr;
}

const wit::core::Disk* FileListView::DiskAt(int row) {
    if (!ShowsDisks()) return nullptr;
    EnsurePage(row);
    const int index = row - pageStart;
    return index >= 0 && index < static_cast<int>(diskPage.size()) ? &diskPage[index] : nullptr;
}

std::wstring FileListView::TextFor(int row, int column) {
    if (ShowsDisks()) {
        const auto* disk = DiskAt(row);
        if (!disk) return L"";
        switch (column) {
        case 0: return disk->diskName;
        case 1: return DiskTypeLabel(disk->diskType);
        case 2: return wit::core::FormatSize(disk->totalCapacity);
        case 3: return wit::core::FormatSize(disk->freeSpace);
        case 4: return wit::platform::FormatUnixTimestamp(disk->updatedAt);
        case 5: return std::to_wstring(disk->diskNumber);
        case 6: return disk->description;
        case 7: return disk->category;
        case 8: return disk->location;
        default: return L"";
        }
    }
    const auto* entry = EntryAt(row);
    if (!entry) return L"";
    const auto& file = *entry;
    switch (column) {
    case 0:
        return file.name;
    case 1:
        return file.isDirectory ? L"Folder" : file.extension;
    case 2:
        return wit::core::FormatSize(file.size);
    case 3:
        return file.parentPath;
    case 4:
        return file.modifiedAt;
    default:
        return L"";
    }
}
}
