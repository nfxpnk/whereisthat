#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <array>
#include <functional>
#include <string>
#include "WtlSupport.h"

namespace wit::app {

enum class AppStatus {
    Idle,
    Busy,
    Searching
};

inline constexpr const wchar_t* kHoveredRowProperty = L"WitHoveredRow";

class MainWindowChrome {
public:
    bool Create(HWND parent, bool showStatusBar, std::function<void()> selectAllAction);
    void Destroy();

    HWND TreeHandle() const { return treeHandle_; }
    HWND FilesHandle() const { return filesHandle_; }
    HWND BackHandle() const { return backHandle_; }
    HWND ForwardHandle() const { return forwardHandle_; }
    HWND AddressHandle() const { return addressHandle_; }
    HWND StatusHandle() const { return statusHandle_; }

    void OnSize(int width, int height);
    bool OnLeftButtonDown(int x, int y);
    bool OnMouseMove(int x);
    bool OnLeftButtonUp();
    void OnCaptureChanged();
    bool OnSetCursor(LPARAM lparam);

    void SetStatusVisible(bool visible);
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

        BEGIN_MSG_MAP(ContentsListSubclass)
            MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
            MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
            MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        END_MSG_MAP()

    private:
        static int HoveredRow(HWND hwnd) {
            return static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(hwnd, kHoveredRowProperty))) - 1;
        }

        static void SetHoveredRow(HWND hwnd, int row) {
            if (row < 0) {
                RemovePropW(hwnd, kHoveredRowProperty);
                return;
            }
            const auto value = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(row + 1));
            SetPropW(hwnd, kHoveredRowProperty, value);
        }

        static void InvalidateRow(HWND hwnd, int row) {
            if (row < 0) return;
            RECT bounds{};
            if (ListView_GetItemRect(hwnd, row, &bounds, LVIR_BOUNDS)) {
                bounds.left = 0;
                RECT client{};
                ::GetClientRect(hwnd, &client);
                bounds.right = client.right;
                ::InvalidateRect(hwnd, &bounds, FALSE);
            }
        }

        static int RowFromPoint(HWND hwnd, LPARAM lparam) {
            LVHITTESTINFO hitTest{};
            hitTest.pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            const int hit = ListView_SubItemHitTest(hwnd, &hitTest);
            if (hit >= 0) return hit;

            RECT client{};
            ::GetClientRect(hwnd, &client);
            if (!PtInRect(&client, hitTest.pt)) return -1;

            const int count = ListView_GetItemCount(hwnd);
            const int top = ListView_GetTopIndex(hwnd);
            if (count <= 0 || top < 0 || top >= count) return -1;

            RECT firstRow{};
            if (!ListView_GetItemRect(hwnd, top, &firstRow, LVIR_BOUNDS)) return -1;
            const int rowHeight = firstRow.bottom - firstRow.top;
            if (rowHeight <= 0 || hitTest.pt.y < firstRow.top) return -1;

            const int row = top + (hitTest.pt.y - firstRow.top) / rowHeight;
            return row >= 0 && row < count ? row : -1;
        }

        void UpdateHoveredRow(int row) {
            const int previous = HoveredRow(m_hWnd);
            if (previous == row) return;
            SetHoveredRow(m_hWnd, row);
            InvalidateRow(m_hWnd, previous);
            InvalidateRow(m_hWnd, row);
        }

        LRESULT OnKeyDown(UINT, WPARAM wparam, LPARAM, BOOL& handled) {
            if (wparam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
                (GetKeyState(VK_MENU) & 0x8000) == 0) {
                if (selectAllAction_ != nullptr && *selectAllAction_) {
                    (*selectAllAction_)();
                }
                return 0;
            }
            handled = FALSE;
            return 0;
        }

        LRESULT OnMouseMove(UINT, WPARAM, LPARAM lparam, BOOL& handled) {
            TRACKMOUSEEVENT trackMouseEvent{sizeof(trackMouseEvent), TME_LEAVE, m_hWnd, 0};
            TrackMouseEvent(&trackMouseEvent);
            UpdateHoveredRow(RowFromPoint(m_hWnd, lparam));
            handled = FALSE;
            return 0;
        }

        LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL& handled) {
            UpdateHoveredRow(-1);
            handled = FALSE;
            return 0;
        }

        std::function<void()>* selectAllAction_{};
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
    AppStatus appStatus_{AppStatus::Idle};
    std::array<std::wstring, 5> statusText_{};
    std::function<void()> selectAllAction_;
    ContentsListSubclass filesSubclass_;

    bool CreateToolbar();
    bool CreateBrowserImages();
    bool IsOverSplitter(int x, int y) const;
    void InvalidateStatusPart(int part);
};

}
