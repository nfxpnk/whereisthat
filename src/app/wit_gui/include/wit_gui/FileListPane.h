#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include "wit_types/BrowserLocation.h"
#include <wit_types/Disk.h>
#include <wit_types/FileEntry.h>
#include "wit_database/Database.h"

namespace wit::ui {
class FileListView {
public:
    HWND hwnd{};
    wit::core::BrowserLocation location;
    wit::storage::Database* db{};
    int total{};
    int pageStart{-1};
    std::vector<wit::core::FileEntry> page;
    std::vector<wit::core::BrowserItem> browserPage;

    void Attach(HWND handle) { hwnd = handle; }
    void SetLocation(const wit::core::BrowserLocation& newLocation, wit::storage::Database* database);
    void EnsurePage(int index);
    bool ShowsBrowserItems() const { return db && (location.isRoot || location.isDiskGroup); }
    bool ShowsDisks() const { return ShowsBrowserItems(); }
    const wit::core::FileEntry* EntryAt(int row);
    const wit::core::BrowserItem* BrowserItemAt(int row);
    const wit::core::Disk* DiskAt(int row);
    int ImageFor(int row);
    std::wstring TextFor(int row, int column);

private:
    void ConfigureColumns();
};
}
#include <Windows.h>
#include <vector>
#include "wit_catalog/Catalog.h"

namespace wit::ui {
class CatalogListView {
public:
    HWND hwnd{};
    std::vector<wit::core::Catalog> catalogs;

    void Attach(HWND handle) { hwnd = handle; }
    void Reload();
};
}


