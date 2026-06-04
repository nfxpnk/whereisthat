#pragma once

#include "wit_win32/BaseWindow.h"
#include "resource.h"

namespace wit::ui {

class AboutDialog : public ATL::CDialogImpl<AboutDialog> {
public:
    enum { IDD = IDD_ABOUT };

    bool Show(HWND owner);

    BEGIN_MSG_MAP(AboutDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        NOTIFY_HANDLER(IDC_ABOUT_TABS, TCN_SELCHANGE, OnTabChanged)
        NOTIFY_HANDLER(IDC_ABOUT_LINK, NM_CLICK, OnLinkActivated)
        NOTIFY_HANDLER(IDC_ABOUT_LINK, NM_RETURN, OnLinkActivated)
        NOTIFY_HANDLER(IDC_ABOUT_REPOSITORY_LINK, NM_CLICK, OnRepositoryLinkActivated)
        NOTIFY_HANDLER(IDC_ABOUT_REPOSITORY_LINK, NM_RETURN, OnRepositoryLinkActivated)
        COMMAND_ID_HANDLER(IDOK, OnClose)
        COMMAND_ID_HANDLER(IDCANCEL, OnClose)
    END_MSG_MAP()

private:
    HFONT titleFont_{};
    HFONT versionFont_{};
    HICON icon_{};

    void ShowActivePage();

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnTabChanged(int idCtrl, LPNMHDR notifyHeader, BOOL& handled);
    LRESULT OnLinkActivated(int idCtrl, LPNMHDR notifyHeader, BOOL& handled);
    LRESULT OnRepositoryLinkActivated(int idCtrl, LPNMHDR notifyHeader, BOOL& handled);
    LRESULT OnClose(WORD notifyCode, WORD id, HWND control, BOOL& handled);
};

}

