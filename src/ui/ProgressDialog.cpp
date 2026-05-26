#include "ProgressDialog.h"
#include <string>
#include <utility>

namespace wit::ui {

bool ProgressDialog::Show(HWND owner, std::function<void()> onCancel) {
    onCancel_ = std::move(onCancel);
    if (!m_hWnd && Create(owner) == nullptr) return false;
    SetDlgItemTextW(IDC_SCAN_STATUS, L"Scanning files and folders...");
    GetDlgItem(IDCANCEL).EnableWindow(TRUE);
    Update(0, 0);
    ShowWindow(SW_SHOW);
    return true;
}

void ProgressDialog::Update(std::uint64_t files, std::uint64_t folders) {
    if (!m_hWnd) return;
    SetDlgItemTextW(IDC_SCAN_FILES, std::to_wstring(files).c_str());
    SetDlgItemTextW(IDC_SCAN_FOLDERS, std::to_wstring(folders).c_str());
}

void ProgressDialog::SetCancelling() {
    if (!m_hWnd) return;
    SetDlgItemTextW(IDC_SCAN_STATUS, L"Cancelling...");
    GetDlgItem(IDCANCEL).EnableWindow(FALSE);
}

void ProgressDialog::Close() {
    if (m_hWnd) DestroyWindow();
    onCancel_ = {};
}

LRESULT ProgressDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    CenterWindow(GetParent());
    return TRUE;
}

LRESULT ProgressDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
    if (onCancel_) onCancel_();
    return 0;
}

}
