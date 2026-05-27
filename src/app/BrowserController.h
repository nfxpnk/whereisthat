#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <functional>
#include <string>
#include <vector>
#include "../core/CatalogIdentity.h"
#include "../storage/Database.h"
#include "../ui/CatalogTreeView.h"
#include "../ui/FileListView.h"

namespace wit::app {

class BrowserController {
public:
    using DatabaseResolver = std::function<wit::storage::Database*(wit::core::CatalogId)>;
    using LabelResolver = std::function<std::wstring(wit::core::CatalogId)>;

    void Attach(HWND tree, HWND files, HWND back, HWND forward, HWND address,
        DatabaseResolver databaseResolver, LabelResolver labelResolver);
    void Clear();
    void AddCatalog(wit::core::CatalogId id, const std::wstring& label,
        wit::storage::Database* database, bool select);
    void RefreshCatalog(wit::core::CatalogId id, const std::wstring& label,
        wit::storage::Database* database, bool select);
    void RemoveCatalog(wit::core::CatalogId id);
    bool SelectCatalogRoot(wit::core::CatalogId id);
    bool SelectFirstCatalogRoot();
    bool IsCatalogRoot(HTREEITEM item) const { return tree_.IsCatalogRoot(item); }
    wit::core::CatalogId SelectedCatalogId() const;
    void NavigateBack();
    void NavigateForward();

    wit::core::CatalogId OnTreeSelectionChanged(LPNMHDR header);
    LRESULT OnTreeExpanding(LPNMHDR header);
    LRESULT OnFileGetDispInfo(LPNMHDR header);
    LRESULT OnFileCustomDraw(LPNMHDR header) const;
    LRESULT OnFileActivate(LPNMHDR header);
    bool FileItemStateChanged(LPNMHDR header) const;
    void SelectAll();

    std::wstring FocusedItemStatus();
    std::wstring SelectionSummaryStatus();

private:
    HWND treeHandle_{};
    HWND filesHandle_{};
    HWND backHandle_{};
    HWND forwardHandle_{};
    HWND addressHandle_{};
    DatabaseResolver databaseResolver_;
    LabelResolver labelResolver_;
    wit::ui::CatalogTreeView tree_;
    wit::ui::FileListView files_;
    wit::core::BrowserTarget currentTarget_;
    std::vector<wit::core::BrowserTarget> history_;
    int historyIndex_{-1};
    bool selectingTree_{};
    bool hasTarget_{};

    void NavigateTo(const wit::core::BrowserTarget& target, bool addToHistory);
    void UpdateNavigationControls();
    std::wstring AddressFor(const wit::core::BrowserTarget& target) const;
};

}
