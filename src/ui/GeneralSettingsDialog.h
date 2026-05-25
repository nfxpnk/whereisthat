#pragma once

#include "../app/WtlSupport.h"
#include "../app/resource.h"
#include "../platform/AppSettings.h"

namespace wit::ui {

class GeneralSettingsDialog : public ATL::CDialogImpl<GeneralSettingsDialog> {
public:
    enum { IDD = IDD_GENERAL_SETTINGS };

    bool Show(HWND owner, const wit::platform::AppSettings& current, wit::platform::AppSettings& accepted);

    BEGIN_MSG_MAP(GeneralSettingsDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDOK, OnConfirm)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

private:
    wit::platform::AppSettings settings_;

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnConfirm(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnCancel(WORD notifyCode, WORD id, HWND control, BOOL& handled);
};

}
