#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <array>
#include <functional>
#include <string>
#include "wit_win32/BaseWindow.h"

namespace wit::app {

enum class AppStatus {
    Idle,
    Busy,
    Searching
};

class MainWindowChrome {
public:
    bool Create(HWND parent, bool showStatusBar, bool showToolbar, int splitterPosition,
        std::function<void()> selectAllAction,
        std::function<bool()> cancelSelectAllOverrideAction);
    void Destroy();

    HWND TreeHandle() const { return treeHandle_; }
    HWND FilesHandle() const { return filesHandle_; }
    HWND BackHandle() const { return backHandle_; }
    HWND ForwardHandle() const { return forwardHandle_; }
    HWND AddressHandle() const { return addressHandle_; }
    HWND StatusHandle() const { return statusHandle_; }
    int SplitterPosition() const { return splitterPosition_; }

    void OnSize(int width, int height);
    bool OnLeftButtonDown(int x, int y);
    bool OnMouseMove(int x);
    bool OnLeftButtonUp();
    void OnCaptureChanged();
    bool OnSetCursor(LPARAM lparam);

    void SetStatusVisible(bool visible);
    void SetToolbarVisible(bool visible);
    void SetStatusText(int part, const std::wstring& text);
    void UpdateCatalogLockStatus();
    void SetAppStatus(AppStatus status);
    void SetScanCommandEnabled(bool enabled);
    void SetSaveCommandEnabled(bool enabled);
    void UpdateProgramStatusLights();
    bool DrawStatusPart(LPDRAWITEMSTRUCT drawItem, bool protectedCatalog);
    LRESULT OnToolbarDropDown(LPNMTOOLBAR notification);
    void SetToolbarTooltip(LPNMTBGETINFOTIPW tip) const;

private:
    class ContentsListSubclass : public ATL::CWindowImpl<ContentsListSubclass, WTL::CListViewCtrl> {
    public:
        void SetAction(std::function<void()>* selectAllAction) {
            selectAllAction_ = selectAllAction;
        }
        void SetCancelAction(std::function<bool()>* cancelSelectAllOverrideAction) {
            cancelSelectAllOverrideAction_ = cancelSelectAllOverrideAction;
        }

        BEGIN_MSG_MAP(ContentsListSubclass)
            MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
            MESSAGE_HANDLER(WM_LBUTTONDOWN, OnPointerDown)
            MESSAGE_HANDLER(WM_RBUTTONDOWN, OnPointerDown)
        END_MSG_MAP()

    private:
        LRESULT OnKeyDown(UINT, WPARAM wparam, LPARAM, BOOL& handled) {
            if (wparam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
                (GetKeyState(VK_MENU) & 0x8000) == 0) {
                if (selectAllAction_ != nullptr && *selectAllAction_) {
                    (*selectAllAction_)();
                }
                return 0;
            }
            if (cancelSelectAllOverrideAction_ != nullptr && *cancelSelectAllOverrideAction_) {
                (*cancelSelectAllOverrideAction_)();
            }
            handled = FALSE;
            return 0;
        }

        LRESULT OnPointerDown(UINT, WPARAM, LPARAM, BOOL& handled) {
            if (cancelSelectAllOverrideAction_ != nullptr && *cancelSelectAllOverrideAction_) {
                (*cancelSelectAllOverrideAction_)();
            }
            handled = FALSE;
            return 0;
        }

        std::function<void()>* selectAllAction_{};
        std::function<bool()>* cancelSelectAllOverrideAction_{};
    };

    static constexpr int kSplitterWidth = 4;
    static constexpr int kMinPaneWidth = 80;
    ATL::CWindow parent_;
    WTL::CTreeViewCtrl treeHandle_;
    WTL::CListViewCtrl filesHandle_;
    WTL::CButton backHandle_;
    WTL::CButton forwardHandle_;
    WTL::CEdit addressHandle_;
    WTL::CStatusBarCtrl statusHandle_;
    WTL::CToolBarCtrl toolbarHandle_;
    HIMAGELIST toolbarImages_{};
    HIMAGELIST browserImages_{};
    int splitterPosition_{-1};
    int splitterDragOffset_{};
    int contentHeight_{};
    int toolbarHeight_{};
    bool splitterDragging_{};
    bool statusVisible_{true};
    bool toolbarVisible_{true};
    AppStatus appStatus_{AppStatus::Idle};
    std::array<std::wstring, 5> statusText_{};
    std::function<void()> selectAllAction_;
    std::function<bool()> cancelSelectAllOverrideAction_;
    ContentsListSubclass filesSubclass_;

    bool CreateToolbar();
    bool CreateBrowserImages();
    bool IsOverSplitter(int x, int y) const;
    void InvalidateStatusPart(int part);
};

}

