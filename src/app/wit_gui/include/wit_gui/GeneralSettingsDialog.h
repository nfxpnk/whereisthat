#pragma once

#include "wit_win32/BaseWindow.h"
#include "resource.h"
#include <wit_infra/AppSettings.h>
#include <functional>

namespace wit::ui {

class GeneralSettingsDialog : public ATL::CDialogImpl<GeneralSettingsDialog> {
public:
    enum { IDD = IDD_GENERAL_SETTINGS };

    enum class ApplyResult {
        Applied,
        Rejected
    };

    using ApplyHandler = std::function<ApplyResult(const wit::platform::AppSettings&)>;

    enum class Page {
        General,
        UserInterface
    };

    // ApplyHandler is the required persistence path for Apply and OK. It owns user-facing errors
    // and presentation updates; Cancel does not call it.
    void Show(HWND owner, const wit::platform::AppSettings& current, ApplyHandler applyHandler);

    BEGIN_MSG_MAP(GeneralSettingsDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        NOTIFY_HANDLER(IDC_SETTINGS_TREE, TVN_SELCHANGEDW, OnTreeSelectionChanged)
        COMMAND_ID_HANDLER(IDOK, OnConfirm)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
        COMMAND_ID_HANDLER(IDC_SETTINGS_APPLY, OnApply)
        COMMAND_ID_HANDLER(IDHELP, OnHelp)
    END_MSG_MAP()

private:
    class GeneralPageDialog : public ATL::CDialogImpl<GeneralPageDialog> {
    public:
        enum { IDD = IDD_SETTINGS_GENERAL_PAGE };

        void SetOwner(GeneralSettingsDialog* owner) noexcept { owner_ = owner; }

        BEGIN_MSG_MAP(GeneralPageDialog)
            COMMAND_HANDLER(IDC_DATE_TIME_FORMAT, CBN_SELCHANGE, OnDateTimeFormatChanged)
            COMMAND_HANDLER(IDC_DATE_TIME_FORMAT, CBN_EDITCHANGE, OnDateTimeFormatChanged)
            COMMAND_HANDLER(IDC_ENABLE_SCAN_FILE_DELAY, BN_CLICKED, OnSettingChanged)
        END_MSG_MAP()

    private:
        GeneralSettingsDialog* owner_{};

        LRESULT OnDateTimeFormatChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
        LRESULT OnSettingChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    };

    class UserInterfacePageDialog : public ATL::CDialogImpl<UserInterfacePageDialog> {
    public:
        enum { IDD = IDD_SETTINGS_USER_INTERFACE_PAGE };

        void SetOwner(GeneralSettingsDialog* owner) noexcept { owner_ = owner; }

        BEGIN_MSG_MAP(UserInterfacePageDialog)
            COMMAND_HANDLER(IDC_SHOW_STATUS_BAR, BN_CLICKED, OnSettingChanged)
            COMMAND_HANDLER(IDC_SHOW_TOOLBAR, BN_CLICKED, OnSettingChanged)
        END_MSG_MAP()

    private:
        GeneralSettingsDialog* owner_{};

        LRESULT OnSettingChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    };

    wit::platform::AppSettings settings_;
    ApplyHandler applyHandler_;
    GeneralPageDialog generalPage_;
    UserInterfacePageDialog userInterfacePage_;
    bool initializing_{};

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnTreeSelectionChanged(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnConfirm(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnCancel(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnApply(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnHelp(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnDateTimeFormatChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnSettingChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    HWND Control(int id) const;
    void CreatePages();
    void PositionPage(HWND page);
    void PopulateTree();
    void SelectInitialPage();
    void ShowPage(Page page);
    bool TryReadControls(wit::platform::AppSettings& settings, bool showValidation);
    bool ApplyChanges(bool closeAfterApply);
    void MarkDirtyIfChanged();
    void SetApplyEnabled(bool enabled);
    std::wstring SelectedDateTimeFormat() const;
    std::wstring ControlText(int id) const;
    void UpdateDateTimeFormatSample();
};

}

