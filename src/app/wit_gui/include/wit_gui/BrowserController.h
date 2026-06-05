#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "wit_types/CatalogIdentity.h"
#include "wit_database/Database.h"
#include "wit_gui/TreeViewPane.h"
#include "wit_gui/FileListPane.h"

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
    void MoveDiskToGroup(wit::core::CatalogId id, std::int64_t diskId,
        std::int64_t diskGroupId, wit::storage::Database* database);
    void RemoveCatalog(wit::core::CatalogId id);
    bool SelectCatalogRoot(wit::core::CatalogId id);
    bool SelectFirstCatalogRoot();
    bool IsCatalogRoot(HTREEITEM item) const { return tree_.IsCatalogRoot(item); }
    [[nodiscard]] std::optional<wit::core::BrowserTarget> TargetForTreeItem(HTREEITEM item) const;
    [[nodiscard]] std::optional<wit::core::BrowserTarget> SelectedTreeTarget() const;
    wit::core::CatalogId SelectedCatalogId() const;
    void NavigateBack();
    void NavigateForward();
    void RefreshDisplay();

    wit::core::CatalogId OnTreeSelectionChanged(LPNMHDR header);
    LRESULT OnTreeExpanding(LPNMHDR header);
    LRESULT OnFileGetDispInfo(LPNMHDR header);
    LRESULT OnFileCacheHint(LPNMHDR header);
    LRESULT OnFileActivate(LPNMHDR header);
    LRESULT OnFileClick(LPNMHDR header);
    LRESULT OnFileOdStateChanged(LPNMHDR header);
    bool FileItemStateChanged(LPNMHDR header);
    void SelectAll();
    void ClearFileSelection();
    void PrepareFileSingleSelection();

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

    void NavigateTo(const wit::core::BrowserTarget& target, bool addToHistory, bool syncTreeSelection = true);
    void UpdateNavigationControls();
    std::wstring AddressFor(const wit::core::BrowserTarget& target) const;
    void UpdateMovedDiskTargets(wit::core::CatalogId id, std::int64_t diskId,
        std::int64_t diskGroupId, const std::wstring& diskGroupName);
};

}

