#include "ProgressDialog.h"
#include <string>
#include <utility>

namespace wit::ui {

bool ProgressDialog::Show(HWND owner, std::function<void()> onCancel) {
    onCancel_ = std::move(onCancel);
    if (!m_hWnd && Create(owner) == nullptr) return false;
    GetDlgItem(IDCANCEL).EnableWindow(TRUE);
    Update(0, 0, 0, 0, false, true);
    ShowWindow(SW_SHOW);
    return true;
}

void ProgressDialog::Update(std::uint64_t files, std::uint64_t folders, std::uint64_t totalFiles,
    std::uint64_t remainingFiles, bool totalKnown, bool counting) {
    if (!m_hWnd) return;
    SetDlgItemTextW(IDC_SCAN_STATUS, counting ? L"Counting files..." : L"Scanning files and folders...");
    const auto filesText = totalKnown
        ? std::to_wstring(files) + L" / " + std::to_wstring(totalFiles)
        : std::to_wstring(files);
    SetDlgItemTextW(IDC_SCAN_FILES, filesText.c_str());
    SetDlgItemTextW(IDC_SCAN_REMAINING, totalKnown ? std::to_wstring(remainingFiles).c_str() : L"-");
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
