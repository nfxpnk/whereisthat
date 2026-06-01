#include "wit_gui/GeneralSettingsDialog.h"
#include <string>

namespace wit::ui {

bool GeneralSettingsDialog::Show(HWND owner, const wit::platform::AppSettings& current,
    wit::platform::AppSettings& accepted) {
    settings_ = current;
    if (DoModal(owner) != IDOK) return false;
    accepted = settings_;
    return true;
}

LRESULT GeneralSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    CheckDlgButton(IDC_SHOW_STATUS_BAR, settings_.showStatusBar ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(IDC_MAIN_SPLITTER_POSITION, std::to_wstring(settings_.mainSplitterPosition).c_str());
    SetDlgItemTextW(IDC_LAST_OPENED_CATALOG, settings_.lastCatalogPath.empty()
        ? L"(No catalog opened)" : settings_.lastCatalogPath.c_str());
    return TRUE;
}

LRESULT GeneralSettingsDialog::OnConfirm(WORD, WORD, HWND, BOOL&) {
    settings_.showStatusBar = IsDlgButtonChecked(IDC_SHOW_STATUS_BAR) == BST_CHECKED;
    EndDialog(IDOK);
    return 0;
}

LRESULT GeneralSettingsDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
    EndDialog(IDCANCEL);
    return 0;
}

}

