#include "wit_gui/GeneralSettingsDialog.h"
#include <format>
#include <string>
#include <array>
#include <wit_infra/Win32Helpers.h>

namespace wit::ui {
namespace {

constexpr const wchar_t* kUseWindowsDefaultLabel = L"Windows default";
constexpr std::array<const wchar_t*, 4> kCommonDateTimeFormats{
    L"YYYY-MM-DD HH:mm:ss",
    L"DD-MM-YYYY HH:mm:ss",
    L"MM/DD/YYYY HH:mm:ss",
    L"DD/MM/YYYY HH:mm:ss",
};

}

bool GeneralSettingsDialog::Show(HWND owner, const wit::platform::AppSettings& current,
    wit::platform::AppSettings& accepted) {
    settings_ = current;
    if (DoModal(owner) != IDOK) return false;
    accepted = settings_;
    return true;
}

LRESULT GeneralSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    CheckDlgButton(IDC_SHOW_STATUS_BAR, settings_.showStatusBar ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_SHOW_TOOLBAR, settings_.showToolbar ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_ENABLE_SCAN_FILE_DELAY,
        settings_.enableScanFileDelay ? BST_CHECKED : BST_UNCHECKED);
    const auto formatCombo = GetDlgItem(IDC_DATE_TIME_FORMAT);
    SendMessageW(formatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kUseWindowsDefaultLabel));
    for (const auto* format : kCommonDateTimeFormats) {
        SendMessageW(formatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format));
    }
    SetDlgItemTextW(IDC_DATE_TIME_FORMAT, settings_.dateTimeFormat.empty()
        ? kUseWindowsDefaultLabel : settings_.dateTimeFormat.c_str());
    UpdateDateTimeFormatSample();
    const auto splitterPosition = std::format(L"{}", settings_.mainSplitterPosition);
    SetDlgItemTextW(IDC_MAIN_SPLITTER_POSITION, splitterPosition.c_str());
    SetDlgItemTextW(IDC_LAST_OPENED_CATALOG, settings_.lastCatalogPath.empty()
        ? L"(No catalog opened)" : settings_.lastCatalogPath.c_str());
    return TRUE;
}

LRESULT GeneralSettingsDialog::OnConfirm(WORD, WORD, HWND, BOOL&) {
    settings_.showStatusBar = IsDlgButtonChecked(IDC_SHOW_STATUS_BAR) == BST_CHECKED;
    settings_.showToolbar = IsDlgButtonChecked(IDC_SHOW_TOOLBAR) == BST_CHECKED;
    settings_.enableScanFileDelay = IsDlgButtonChecked(IDC_ENABLE_SCAN_FILE_DELAY) == BST_CHECKED;
    const auto dateTimeFormat = SelectedDateTimeFormat();
    if (!wit::platform::IsValidDateTimeFormat(dateTimeFormat)) {
        ::MessageBoxW(m_hWnd, L"Use tokens like YYYY, MM, DD, HH, mm, and ss. Example: YYYY-MM-DD HH:mm:ss.",
            L"Date and Time Format", MB_OK | MB_ICONWARNING);
        ::SetFocus(GetDlgItem(IDC_DATE_TIME_FORMAT));
        return 0;
    }
    settings_.dateTimeFormat = dateTimeFormat;
    EndDialog(IDOK);
    return 0;
}

LRESULT GeneralSettingsDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT GeneralSettingsDialog::OnDateTimeFormatChanged(WORD, WORD, HWND, BOOL&) {
    UpdateDateTimeFormatSample();
    return 0;
}

std::wstring GeneralSettingsDialog::SelectedDateTimeFormat() const {
    const auto length = ::GetWindowTextLengthW(GetDlgItem(IDC_DATE_TIME_FORMAT));
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    GetDlgItemTextW(IDC_DATE_TIME_FORMAT, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(length));
    if (value == kUseWindowsDefaultLabel) return {};
    return value;
}

void GeneralSettingsDialog::UpdateDateTimeFormatSample() {
    const auto pattern = SelectedDateTimeFormat();
    const auto sample = wit::platform::IsValidDateTimeFormat(pattern)
        ? wit::platform::FormatDateTimeSample(pattern) : std::wstring(L"Invalid format");
    SetDlgItemTextW(IDC_DATE_TIME_FORMAT_SAMPLE, sample.c_str());
}

}

