#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include "WtlSupport.h"
#include "BrowserController.h"
#include "CatalogSession.h"
#include "MainWindowChrome.h"
#include "ScanCoordinator.h"
#include "resource.h"

class MainFrame : public WTL::CFrameWindowImpl<MainFrame>, public WTL::CMessageFilter {
public:
    DECLARE_FRAME_WND_CLASS(L"WhereIsThatMainFrame", IDI_APPICON)

    bool Create();
    void Show(int command);
    HWND WindowHandle() const { return m_hWnd; }
    BOOL PreTranslateMessage(MSG* message) override {
        return WTL::CFrameWindowImpl<MainFrame>::PreTranslateMessage(message);
    }

    BEGIN_MSG_MAP(MainFrame)
        MESSAGE_HANDLER(WM_CREATE, OnFrameMessage)
        MESSAGE_HANDLER(WM_SIZE, OnFrameMessage)
        MESSAGE_HANDLER(WM_CLOSE, OnFrameMessage)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnFrameMessage)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnFrameMessage)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnFrameMessage)
        MESSAGE_HANDLER(WM_CAPTURECHANGED, OnFrameMessage)
        MESSAGE_HANDLER(WM_SETCURSOR, OnFrameMessage)
        MESSAGE_HANDLER(WM_COMMAND, OnFrameMessage)
        MESSAGE_HANDLER(WM_DRAWITEM, OnFrameMessage)
        MESSAGE_HANDLER(WM_NOTIFY, OnFrameMessage)
        MESSAGE_HANDLER(wit::app::WM_SCAN_PROGRESS, OnFrameMessage)
        MESSAGE_HANDLER(wit::app::WM_SCAN_COMPLETE, OnFrameMessage)
        MESSAGE_HANDLER(WM_DESTROY, OnFrameMessage)
        CHAIN_MSG_MAP(WTL::CFrameWindowImpl<MainFrame>)
    END_MSG_MAP()

private:
    wit::app::MainWindowChrome chrome_;
    wit::app::BrowserController browser_;
    wit::app::CatalogSession session_;
    wit::app::ScanCoordinator scans_;
    wit::app::ScanId activeScanId_{};
    bool closePending_{};

    LRESULT OnFrameMessage(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    bool OnCreate();
    void OnClose();
    void OnDestroy();
    void OnCommand(int id);
    void OnNewCatalog();
    void OnOpenCatalog();
    void OnOpenRecentCatalog(int commandId);
    void OnAddOrUpdateDiskImage();
    bool OnSaveCatalog();
    bool OnSaveCatalog(wit::app::OpenCatalog& catalog);
    void OnCloseCatalog();
    void OnSearchForItems();
    void OnGeneralSettings();
    void OnExit();
    void OnAbout();
    LRESULT OnTreeRightClick();
    bool ActivateCatalog(const std::wstring& path, bool createNew, bool persistPath);
    wit::app::OpenCatalog* GetActiveCatalog();
    bool ConfirmPendingChanges(wit::app::OpenCatalog& catalog);
    bool ConfirmAllPendingChanges();
    void DiscardPendingChanges(wit::app::OpenCatalog& catalog);
    bool CloseCatalog(wit::app::OpenCatalog& catalog);
    void RefreshOpenRecentMenu();
    void RefreshCatalogStatus();
    void RefreshBrowserStatus();
    void RefreshCatalogCommands();
    void SetScanControlsEnabled(bool enabled);
};
