#include "wit_gui/GeneralSettingsDialog.h"

#include <CommCtrl.h>
#include <array>
#include <format>
#include <span>
#include <string>
#include <utility>
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

constexpr std::array kGeneralPageControls{
    IDC_SETTINGS_GROUP_DATE_TIME,
    IDC_SETTINGS_LABEL_DATE_TIME,
    IDC_DATE_TIME_FORMAT,
    IDC_DATE_TIME_FORMAT_SAMPLE,
    IDC_SETTINGS_GROUP_CATALOG,
    IDC_SETTINGS_LABEL_LAST_CATALOG,
    IDC_LAST_OPENED_CATALOG,
    IDC_SETTINGS_GROUP_DEBUG,
    IDC_ENABLE_SCAN_FILE_DELAY,
};

constexpr std::array kUserInterfacePageControls{
    IDC_SETTINGS_GROUP_DISPLAY,
    IDC_SHOW_STATUS_BAR,
    IDC_SHOW_TOOLBAR,
    IDC_SETTINGS_GROUP_LAYOUT,
    IDC_SETTINGS_LABEL_SPLITTER,
    IDC_MAIN_SPLITTER_POSITION,
};

bool SameEditableSettings(const wit::platform::AppSettings& left, const wit::platform::AppSettings& right) {
    return left.showStatusBar == right.showStatusBar &&
        left.showToolbar == right.showToolbar &&
        left.enableScanFileDelay == right.enableScanFileDelay &&
        left.dateTimeFormat == right.dateTimeFormat;
}

void SetVisible(HWND dialog, int id, bool visible) {
    if (const auto control = GetDlgItem(dialog, id)) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

void SetControlsVisible(HWND dialog, std::span<const int> ids, bool visible) {
    for (const auto id : ids) SetVisible(dialog, id, visible);
}

void InsertTreeItem(HWND tree, std::wstring text, GeneralSettingsDialog::Page page) {
    TVINSERTSTRUCTW item{};
    item.hParent = TVI_ROOT;
    item.hInsertAfter = TVI_LAST;
    item.item.mask = TVIF_TEXT | TVIF_PARAM;
    item.item.pszText = text.data();
    item.item.lParam = static_cast<LPARAM>(page);
    TreeView_InsertItem(tree, &item);
}

}

void GeneralSettingsDialog::Show(HWND owner, const wit::platform::AppSettings& current, ApplyHandler applyHandler) {
    settings_ = current;
    applyHandler_ = std::move(applyHandler);
    DoModal(owner);
}

LRESULT GeneralSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    initializing_ = true;
    SetWindowTextW(L"Settings");
    headerBrush_.Reset(CreateSolidBrush(RGB(226, 234, 246)));

    CheckDlgButton(IDC_SHOW_STATUS_BAR, settings_.showStatusBar ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_SHOW_TOOLBAR, settings_.showToolbar ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_ENABLE_SCAN_FILE_DELAY,
        settings_.enableScanFileDelay ? BST_CHECKED : BST_UNCHECKED);

    const auto formatCombo = GetDlgItem(IDC_DATE_TIME_FORMAT);
    SendMessageW(formatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kUseWindowsDefaultLabel));
    for (const auto* format : kCommonDateTimeFormats) {
        SendMessageW(formatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format));
    }
    const auto* visibleFormat = settings_.dateTimeFormat.empty()
        ? kUseWindowsDefaultLabel : settings_.dateTimeFormat.c_str();
    if (SendMessageW(formatCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1),
        reinterpret_cast<LPARAM>(visibleFormat)) == CB_ERR) {
        SetDlgItemTextW(IDC_DATE_TIME_FORMAT, visibleFormat);
    }
    UpdateDateTimeFormatSample();

    const auto splitterPosition = std::format(L"{}", settings_.mainSplitterPosition);
    SetDlgItemTextW(IDC_MAIN_SPLITTER_POSITION, splitterPosition.c_str());
    SetDlgItemTextW(IDC_LAST_OPENED_CATALOG, settings_.lastCatalogPath.c_str());

    PopulateTree();
    SelectInitialPage();
    ShowPage(Page::General);
    ::SetFocus(GetDlgItem(IDC_SETTINGS_TREE));
    SetApplyEnabled(false);
    initializing_ = false;
    return FALSE;
}

LRESULT GeneralSettingsDialog::OnControlColorStatic(UINT, WPARAM wparam, LPARAM lparam, BOOL& handled) {
    const auto control = reinterpret_cast<HWND>(lparam);
    if (control == GetDlgItem(IDC_SETTINGS_HEADER) || control == GetDlgItem(IDC_SETTINGS_HEADER_TITLE)) {
        const auto dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, RGB(226, 234, 246));
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        return reinterpret_cast<LRESULT>(HeaderBrush());
    }
    handled = FALSE;
    return 0;
}

LRESULT GeneralSettingsDialog::OnTreeSelectionChanged(int, LPNMHDR header, BOOL&) {
    const auto* changed = reinterpret_cast<NMTREEVIEWW*>(header);
    ShowPage(static_cast<Page>(changed->itemNew.lParam));
    return 0;
}

LRESULT GeneralSettingsDialog::OnConfirm(WORD, WORD, HWND, BOOL&) {
    if (!ApplyChanges(true)) return 0;
    EndDialog(IDOK);
    return 0;
}

LRESULT GeneralSettingsDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT GeneralSettingsDialog::OnApply(WORD, WORD, HWND, BOOL&) {
    ApplyChanges(false);
    return 0;
}

LRESULT GeneralSettingsDialog::OnHelp(WORD, WORD, HWND, BOOL&) {
    MessageBoxW(L"No help topic is available for this settings page yet.", L"Settings Help",
        MB_OK | MB_ICONINFORMATION);
    return 0;
}

LRESULT GeneralSettingsDialog::OnDateTimeFormatChanged(WORD notifyCode, WORD, HWND, BOOL&) {
    if (notifyCode == CBN_SELCHANGE) {
        const auto formatCombo = GetDlgItem(IDC_DATE_TIME_FORMAT);
        const auto selection = SendMessageW(formatCombo, CB_GETCURSEL, 0, 0);
        if (selection != CB_ERR) {
            const auto length = SendMessageW(formatCombo, CB_GETLBTEXTLEN, selection, 0);
            if (length != CB_ERR) {
                std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
                SendMessageW(formatCombo, CB_GETLBTEXT, selection, reinterpret_cast<LPARAM>(value.data()));
                value.resize(static_cast<std::size_t>(length));
                SetDlgItemTextW(IDC_DATE_TIME_FORMAT, value.c_str());
            }
        }
    }
    UpdateDateTimeFormatSample();
    MarkDirtyIfChanged();
    return 0;
}

LRESULT GeneralSettingsDialog::OnSettingChanged(WORD, WORD, HWND, BOOL&) {
    MarkDirtyIfChanged();
    return 0;
}

void GeneralSettingsDialog::PopulateTree() {
    const auto tree = GetDlgItem(IDC_SETTINGS_TREE);
    TreeView_DeleteAllItems(tree);
    InsertTreeItem(tree, L"General Settings", Page::General);
    InsertTreeItem(tree, L"User Interface Setup", Page::UserInterface);
}

void GeneralSettingsDialog::SelectInitialPage() {
    const auto tree = GetDlgItem(IDC_SETTINGS_TREE);
    TreeView_SelectItem(tree, TreeView_GetRoot(tree));
}

void GeneralSettingsDialog::ShowPage(Page page) {
    const bool showGeneral = page == Page::General;
    const bool showUserInterface = page == Page::UserInterface;

    SetDlgItemTextW(IDC_SETTINGS_HEADER_TITLE,
        showUserInterface ? L"User Interface Setup" : L"General Settings");

    SetControlsVisible(m_hWnd, kGeneralPageControls, false);
    SetControlsVisible(m_hWnd, kUserInterfacePageControls, false);
    if (showGeneral) {
        SetControlsVisible(m_hWnd, kGeneralPageControls, true);
    } else {
        SetControlsVisible(m_hWnd, kUserInterfacePageControls, true);
    }
}

bool GeneralSettingsDialog::TryReadControls(wit::platform::AppSettings& settings, bool showValidation) {
    settings = settings_;
    settings.showStatusBar = IsDlgButtonChecked(IDC_SHOW_STATUS_BAR) == BST_CHECKED;
    settings.showToolbar = IsDlgButtonChecked(IDC_SHOW_TOOLBAR) == BST_CHECKED;
    settings.enableScanFileDelay = IsDlgButtonChecked(IDC_ENABLE_SCAN_FILE_DELAY) == BST_CHECKED;
    settings.dateTimeFormat = SelectedDateTimeFormat();

    if (!wit::platform::IsValidDateTimeFormat(settings.dateTimeFormat)) {
        if (showValidation) {
            MessageBoxW(L"Use tokens like YYYY, MM, DD, HH, mm, and ss. Example: YYYY-MM-DD HH:mm:ss.",
                L"Date and Time Format", MB_OK | MB_ICONWARNING);
            ::SetFocus(GetDlgItem(IDC_DATE_TIME_FORMAT));
        }
        return false;
    }
    return true;
}

bool GeneralSettingsDialog::ApplyChanges(bool closeAfterApply) {
    wit::platform::AppSettings updated;
    if (!TryReadControls(updated, true)) return false;
    if (!SameEditableSettings(updated, settings_)) {
        if (!applyHandler_ || !applyHandler_(updated)) return false;
        settings_ = std::move(updated);
    }
    SetApplyEnabled(false);
    if (!closeAfterApply) ::SetFocus(GetDlgItem(IDC_SETTINGS_APPLY));
    return true;
}

void GeneralSettingsDialog::MarkDirtyIfChanged() {
    if (initializing_) return;
    wit::platform::AppSettings updated;
    const bool valid = TryReadControls(updated, false);
    SetApplyEnabled(!valid || !SameEditableSettings(updated, settings_));
}

void GeneralSettingsDialog::SetApplyEnabled(bool enabled) {
    ::EnableWindow(GetDlgItem(IDC_SETTINGS_APPLY), enabled ? TRUE : FALSE);
}

HBRUSH GeneralSettingsDialog::HeaderBrush() const noexcept {
    return headerBrush_ ? static_cast<HBRUSH>(headerBrush_.Get()) : GetSysColorBrush(COLOR_BTNFACE);
}

std::wstring GeneralSettingsDialog::SelectedDateTimeFormat() const {
    auto value = ControlText(IDC_DATE_TIME_FORMAT);
    if (value == kUseWindowsDefaultLabel) return {};
    return value;
}

std::wstring GeneralSettingsDialog::ControlText(int id) const {
    const auto control = GetDlgItem(id);
    const auto length = ::GetWindowTextLengthW(control);
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    GetDlgItemTextW(id, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(length));
    return value;
}

void GeneralSettingsDialog::UpdateDateTimeFormatSample() {
    const auto pattern = SelectedDateTimeFormat();
    const auto sample = wit::platform::IsValidDateTimeFormat(pattern)
        ? wit::platform::FormatDateTimeSample(pattern) : std::wstring(L"Invalid format");
    SetDlgItemTextW(IDC_DATE_TIME_FORMAT_SAMPLE, sample.c_str());
}

}
