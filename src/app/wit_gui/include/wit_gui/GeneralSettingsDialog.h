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
        UserInterface,
        FileList,
        DiskImage,
        Description
    };

    // ApplyHandler is the required persistence path for Apply and OK. It owns user-facing errors
    // and presentation updates; closing the window without Apply or OK does not call it.
    bool Show(HWND owner, const wit::platform::AppSettings& current, Page initialPage, ApplyHandler applyHandler);
    void Close();
    HWND WindowHandle() const noexcept { return m_hWnd; }
    BOOL PreTranslateMessage(MSG* message);

    BEGIN_MSG_MAP(GeneralSettingsDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnWindowClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        NOTIFY_HANDLER(IDC_SETTINGS_TREE, TVN_SELCHANGEDW, OnTreeSelectionChanged)
        COMMAND_ID_HANDLER(IDOK, OnConfirm)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
        COMMAND_ID_HANDLER(IDC_SETTINGS_APPLY, OnApply)
        COMMAND_ID_HANDLER(IDHELP, OnHelp)
    END_MSG_MAP()

private:
    template <typename T, UINT DialogId>
    class SettingsPageDialog : public ATL::CDialogImpl<T> {
    public:
        enum { IDD = DialogId };

        void SetOwner(GeneralSettingsDialog* owner) noexcept { owner_ = owner; }

    protected:
        GeneralSettingsDialog* Owner() const noexcept { return owner_; }
        LRESULT OnSettingChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled) {
            return owner_ ? owner_->OnSettingChanged(notifyCode, id, control, handled) : 0;
        }

    private:
        GeneralSettingsDialog* owner_{};
    };

    class GeneralPageDialog : public SettingsPageDialog<GeneralPageDialog, IDD_SETTINGS_GENERAL_PAGE> {
    public:
        BEGIN_MSG_MAP(GeneralPageDialog)
            COMMAND_HANDLER(IDC_DATE_TIME_FORMAT, CBN_SELCHANGE, OnDateTimeFormatChanged)
            COMMAND_HANDLER(IDC_DATE_TIME_FORMAT, CBN_EDITCHANGE, OnDateTimeFormatChanged)
            COMMAND_HANDLER(IDC_ENABLE_SCAN_FILE_DELAY, BN_CLICKED, OnSettingChanged)
        END_MSG_MAP()

    private:
        LRESULT OnDateTimeFormatChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    };

    class UserInterfacePageDialog : public SettingsPageDialog<UserInterfacePageDialog, IDD_SETTINGS_USER_INTERFACE_PAGE> {
    public:
        BEGIN_MSG_MAP(UserInterfacePageDialog)
            COMMAND_HANDLER(IDC_SHOW_STATUS_BAR, BN_CLICKED, OnSettingChanged)
            COMMAND_HANDLER(IDC_SHOW_TOOLBAR, BN_CLICKED, OnSettingChanged)
        END_MSG_MAP()
    };

    wit::platform::AppSettings settings_;
    ApplyHandler applyHandler_;
    GeneralPageDialog generalPage_;
    UserInterfacePageDialog userInterfacePage_;
    HWND launchOwner_{};
    Page initialPage_{Page::General};
    bool initializing_{};

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnWindowClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
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
    void SelectPage(Page page);
    void ShowPage(Page page);
    void LoadSettingsIntoControls(const wit::platform::AppSettings& settings, bool preservePendingEdits);
    void SetDateTimeFormatControl(const std::wstring& format);
    bool TryReadControls(wit::platform::AppSettings& settings, bool showValidation);
    bool ApplyChanges(bool closeAfterApply);
    void MarkDirtyIfChanged();
    void SetApplyEnabled(bool enabled);
    std::wstring SelectedDateTimeFormat() const;
    std::wstring ControlText(int id) const;
    void UpdateDateTimeFormatSample();
};

}

