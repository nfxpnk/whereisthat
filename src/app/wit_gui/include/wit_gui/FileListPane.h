#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
#include "wit_types/BrowserLocation.h"
#include <wit_types/Disk.h>
#include <wit_types/FileEntry.h>
#include "wit_database/IBrowserRepository.h"

namespace wit::ui {
class FileListView {
public:
    HWND hwnd{};
    wit::core::BrowserLocation location;
    wit::storage::IBrowserRepository* browser{};
    int total{};
    int browserPageStart{-1};
    std::vector<wit::core::BrowserItem> browserPage;

    void Attach(HWND handle) { hwnd = handle; }
    void SetLocation(const wit::core::BrowserLocation& newLocation, wit::storage::IBrowserRepository* repository);
    void ResetCachedItems();
    void PreloadRange(int firstRow, int lastRow);
    void ClearSelection();
    void SelectAllItems();
    void SelectOnly(int row);
    void SetSelectionRange(int firstRow, int lastRow, bool selected);
    bool IsSelected(int row) const;
    int SelectedCount() const { return selectedCount_; }
    bool AllItemsSelected() const { return selectAll_; }
    std::vector<int> SelectedRows(int maxRows) const;
    bool ShowsBrowserItems() const { return browser && (location.isRoot || location.isDiskGroup); }
    bool ShowsDisks() const { return ShowsBrowserItems(); }
    const wit::core::FileEntry* EntryAt(int row);
    const wit::core::BrowserItem* BrowserItemAt(int row);
    const wit::core::Disk* DiskAt(int row);
    int ImageFor(int row);
    void TextFor(int row, int column, wchar_t* buffer, std::size_t bufferSize);

private:
    struct CachedFilePage {
        int start{};
        std::vector<wit::core::FileEntry> items;
        unsigned long long lastUsed{};
    };

    static constexpr int PageSize = 512;
    static constexpr std::size_t MaxCachedPages = 16;

    unsigned long long cacheClock_{};
    std::vector<CachedFilePage> cachedFilePages_;
    bool selectAll_{};
    int selectedCount_{};
    std::unordered_set<int> selectedIndices_;
    std::unordered_set<int> deselectedIndices_;

    void ConfigureColumns();
    void ClearCache();
    void CacheFilePage(int pageStart);
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


