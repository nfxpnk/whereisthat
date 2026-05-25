#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include <vector>
#include "../core/BrowserLocation.h"
#include "../storage/Database.h"
#include "../ui/CatalogTreeView.h"
#include "../ui/FileListView.h"

namespace wit::app {

class BrowserController {
public:
    void Attach(HWND tree, HWND files, HWND back, HWND forward, HWND address);
    void Clear();
    void Reload(const std::wstring& catalogLabel, wit::storage::Database* database);
    void NavigateBack(wit::storage::Database* database, const std::wstring& catalogLabel);
    void NavigateForward(wit::storage::Database* database, const std::wstring& catalogLabel);

    LRESULT OnTreeSelectionChanged(LPNMHDR header, wit::storage::Database* database,
        const std::wstring& catalogLabel);
    LRESULT OnTreeExpanding(LPNMHDR header);
    LRESULT OnFileGetDispInfo(LPNMHDR header);
    LRESULT OnFileActivate(LPNMHDR header, wit::storage::Database* database,
        const std::wstring& catalogLabel);
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
    wit::ui::CatalogTreeView tree_;
    wit::ui::FileListView files_;
    wit::core::BrowserLocation currentLocation_;
    std::vector<wit::core::BrowserLocation> history_;
    int historyIndex_{-1};
    bool selectingTree_{};

    void NavigateTo(const wit::core::BrowserLocation& location, bool addToHistory,
        wit::storage::Database* database, const std::wstring& catalogLabel);
    void UpdateNavigationControls();
    std::wstring AddressFor(const wit::core::BrowserLocation& location,
        const std::wstring& catalogLabel) const;
};

}
