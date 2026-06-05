#pragma once

#include "wit_win32/BaseWindow.h"
#include "resource.h"
#include <wit_infra/AppSettings.h>
#include <functional>

namespace wit::ui {

class GeneralSettingsDialog : public ATL::CDialogImpl<GeneralSettingsDialog> {
public:
    enum { IDD = IDD_GENERAL_SETTINGS };

    using ApplyHandler = std::function<bool(const wit::platform::AppSettings&)>;

    enum class Page {
        General,
        UserInterface
    };

    bool Show(HWND owner, const wit::platform::AppSettings& current, wit::platform::AppSettings& accepted,
        ApplyHandler applyHandler = {});

    BEGIN_MSG_MAP(GeneralSettingsDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnControlColorStatic)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        NOTIFY_HANDLER(IDC_SETTINGS_TREE, TVN_SELCHANGEDW, OnTreeSelectionChanged)
        COMMAND_ID_HANDLER(IDOK, OnConfirm)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
        COMMAND_ID_HANDLER(IDC_SETTINGS_APPLY, OnApply)
        COMMAND_ID_HANDLER(IDHELP, OnHelp)
        COMMAND_HANDLER(IDC_DATE_TIME_FORMAT, CBN_SELCHANGE, OnDateTimeFormatChanged)
        COMMAND_HANDLER(IDC_DATE_TIME_FORMAT, CBN_EDITCHANGE, OnDateTimeFormatChanged)
        COMMAND_HANDLER(IDC_SHOW_STATUS_BAR, BN_CLICKED, OnSettingChanged)
        COMMAND_HANDLER(IDC_SHOW_TOOLBAR, BN_CLICKED, OnSettingChanged)
        COMMAND_HANDLER(IDC_ENABLE_SCAN_FILE_DELAY, BN_CLICKED, OnSettingChanged)
        COMMAND_HANDLER(IDC_MAIN_SPLITTER_POSITION, EN_CHANGE, OnSettingChanged)
        COMMAND_HANDLER(IDC_LAST_OPENED_CATALOG, EN_CHANGE, OnSettingChanged)
    END_MSG_MAP()

private:
    wit::platform::AppSettings settings_;
    ApplyHandler applyHandler_;
    HBRUSH headerBrush_{};
    POINT minimumTrackSize_{};
    bool accepted_{};
    bool initializing_{};

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnPaint(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnControlColorStatic(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnTreeSelectionChanged(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnConfirm(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnCancel(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnApply(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnHelp(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnDateTimeFormatChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnSettingChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    void PopulateTree();
    void SelectInitialPage();
    void ShowPage(Page page);
    void LayoutControls();
    void DrawPanelChrome(HDC dc);
    bool TryReadControls(wit::platform::AppSettings& settings, bool showValidation);
    bool ApplyChanges(bool closeAfterApply);
    void MarkDirtyIfChanged();
    void SetApplyEnabled(bool enabled);
    std::wstring SelectedDateTimeFormat() const;
    std::wstring ControlText(int id) const;
    void UpdateDateTimeFormatSample();
};

}

