#pragma once
#include "../app/WtlSupport.h"
#include "../app/resource.h"
#include <cstdint>
#include <functional>

namespace wit::ui {
class ProgressDialog : public ATL::CDialogImpl<ProgressDialog> {
public:
    enum { IDD = IDD_SCAN_PROGRESS };

    bool Show(HWND owner, std::function<void()> onCancel);
    void Update(std::uint64_t files, std::uint64_t folders);
    void SetCancelling();
    void Close();

    BEGIN_MSG_MAP(ProgressDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

private:
    std::function<void()> onCancel_;

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCancel(WORD notifyCode, WORD id, HWND control, BOOL& handled);
};
}
