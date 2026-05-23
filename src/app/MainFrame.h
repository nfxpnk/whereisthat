#pragma once
#include <Windows.h>
#include <CommCtrl.h>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include "../storage/Database.h"
#include "../core/FileScanner.h"
#include "../core/BrowserLocation.h"
#include "../platform/AppSettings.h"
#include "../ui/CatalogTreeView.h"
#include "../ui/FileListView.h"
#include "resource.h"

class MainFrame {
public:
    bool Create();
    void Show(int command);
    HWND WindowHandle() const { return hwnd_; }
private:
    static constexpr wchar_t kClassName[] = L"WhereIsThatMainFrame";
    static constexpr int kSplitterWidth = 4;
    static constexpr int kMinPaneWidth = 80;
    HWND hwnd_{};
    HWND treeCtl_{};
    HWND filesCtl_{};
    HWND backCtl_{};
    HWND forwardCtl_{};
    HWND addressCtl_{};
    HWND status_{};
    HWND toolbar_{};
    HIMAGELIST toolbarImages_{};
    int splitterPosition_{-1};
    int splitterDragOffset_{};
    int contentHeight_{};
    int toolbarHeight_{};
    bool splitterDragging_{};
    std::wstring activeCatalogPath_;
    wit::storage::Database db_;
    wit::ui::CatalogTreeView tree_;
    wit::ui::FileListView files_;
    wit::platform::AppSettings settings_;
    std::thread worker_;
    wit::core::BrowserLocation currentLocation_;
    std::vector<wit::core::BrowserLocation> history_;
    int historyIndex_{-1};
    bool selectingTree_{};

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    bool OnCreate();
    bool CreateToolbar();
    void OnSize(int width, int height);
    bool IsOverSplitter(int x, int y) const;
    void OnDestroy();
    void OnCommand(int id);
    void OnNewCatalog();
    void OnOpenCatalog();
    void OnOpenRecentCatalog(int commandId);
    void OnAddOrUpdateDiskImage();
    void OnSearchForItems();
    void OnGeneralSettings();
    void ApplyDisplaySettings();
    void OnExit();
    void OnAbout();
    LRESULT OnTreeSelectionChanged(LPNMHDR hdr);
    LRESULT OnTreeExpanding(LPNMHDR hdr);
    LRESULT OnFileGetDispInfo(LPNMHDR hdr);
    LRESULT OnFileActivate(LPNMHDR hdr);
    LRESULT OnToolbarDropDown(LPNMTOOLBAR notification);
    bool ActivateCatalog(const std::wstring& path, bool createNew, bool persistPath);
    void RefreshOpenRecentMenu();
    void ClearCatalogViews();
    void ReloadBrowser();
    void NavigateTo(const wit::core::BrowserLocation& location, bool addToHistory);
    void NavigateBack();
    void NavigateForward();
    std::wstring CatalogLabel() const;
    std::wstring AddressFor(const wit::core::BrowserLocation& location) const;
    void UpdateNavigationControls();
};
