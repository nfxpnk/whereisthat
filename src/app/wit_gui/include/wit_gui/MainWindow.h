#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include <vector>
#include "wit_win32/BaseWindow.h"
#include "wit_gui/BrowserController.h"
#include <wit_gui/CatalogWorkflowController.h>
#include <wit_gui/MainWindowChrome.h>
#include "resource.h"
#include "wit_gui/ProgressDialog.h"
#include "wit_gui/SearchPane.h"

class MainFrame : public WTL::CFrameWindowImpl<MainFrame>, public WTL::CMessageFilter {
public:
    DECLARE_FRAME_WND_CLASS(L"WhereIsThatMainFrame", IDI_APPICON)

    explicit MainFrame(std::wstring startupCatalogPath = {});
    bool Create();
    void Show(int command);
    HWND WindowHandle() const { return m_hWnd; }
    BOOL PreTranslateMessage(MSG* message) override {
        if (searchDialog_.PreTranslateMessage(message)) return TRUE;
        return WTL::CFrameWindowImpl<MainFrame>::PreTranslateMessage(message);
    }

    BEGIN_MSG_MAP(MainFrame)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLeftButtonDown)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLeftButtonUp)
        MESSAGE_HANDLER(WM_CAPTURECHANGED, OnCaptureChanged)
        MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
        MESSAGE_HANDLER(WM_COMMAND, OnCommandMessage)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        NOTIFY_HANDLER(IDC_BROWSER_TREE, TVN_SELCHANGEDW, OnTreeSelectionChanged)
        NOTIFY_HANDLER(IDC_BROWSER_TREE, TVN_ITEMEXPANDINGW, OnTreeExpanding)
        NOTIFY_HANDLER(IDC_BROWSER_TREE, NM_CLICK, OnTreeClick)
        NOTIFY_HANDLER(IDC_BROWSER_TREE, NM_RCLICK, OnTreeRightClick)
        NOTIFY_HANDLER(IDC_FILES, LVN_GETDISPINFOW, OnFileGetDispInfo)
        NOTIFY_HANDLER(IDC_FILES, LVN_ODCACHEHINT, OnFileCacheHint)
        NOTIFY_HANDLER(IDC_FILES, LVN_ODSTATECHANGED, OnFileOdStateChanged)
        NOTIFY_HANDLER(IDC_FILES, LVN_ITEMACTIVATE, OnFileActivate)
        NOTIFY_HANDLER(IDC_FILES, NM_CLICK, OnFileClick)
        NOTIFY_HANDLER(IDC_FILES, LVN_ITEMCHANGED, OnFileItemChanged)
        NOTIFY_HANDLER(IDC_TOOLBAR, TBN_DROPDOWN, OnToolbarDropDown)
        NOTIFY_HANDLER(IDC_TOOLBAR, TBN_GETINFOTIPW, OnToolbarGetInfoTip)
        MESSAGE_HANDLER(wit::app::WM_SCAN_PROGRESS, OnScanProgress)
        MESSAGE_HANDLER(wit::app::WM_SCAN_COMPLETE, OnScanComplete)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        CHAIN_MSG_MAP(WTL::CFrameWindowImpl<MainFrame>)
    END_MSG_MAP()

private:
    wit::app::MainWindowChrome chrome_;
    wit::app::BrowserController browser_;
    wit::app::CatalogWorkflowController controller_;
    wit::ui::ProgressDialog scanProgressDialog_;
    wit::ui::SearchDialog searchDialog_;
    std::wstring startupCatalogPath_;
    bool protectedCatalog_{};

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnLeftButtonDown(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnMouseMove(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnLeftButtonUp(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCaptureChanged(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSetCursor(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCommandMessage(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDrawItem(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnScanProgress(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnScanComplete(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnTreeSelectionChanged(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnTreeExpanding(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnTreeClick(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnTreeRightClick(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnFileGetDispInfo(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnFileCacheHint(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnFileOdStateChanged(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnFileActivate(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnFileClick(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnFileItemChanged(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnToolbarDropDown(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnToolbarGetInfoTip(int id, LPNMHDR header, BOOL& handled);
    bool InitializeFrame();
    void RequestClose();
    void CleanupFrame();
    void HandleCommand(int id);
    void OnExit();
    void OnAbout();
    void OnMoveSelectedItemToGroup();
    LRESULT ShowTreeContextMenu();
    void ApplyControllerResult(wit::app::ControllerResult result);
    void PerformRequest(const wit::app::RequestEffect& request);
    void RenderRecentMenu(const std::vector<std::wstring>& paths);
    void RenderPresentation(const wit::app::PresentationEffect& presentation);
    void UpdateBrowserStatus();
};


