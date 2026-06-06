#include "wit_gui/GeneralSettingsDialog.h"

#include <CommCtrl.h>
#include <array>
#include <format>
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

bool SameEditableSettings(const wit::platform::AppSettings& left, const wit::platform::AppSettings& right) {
    return left.showStatusBar == right.showStatusBar &&
        left.showToolbar == right.showToolbar &&
        left.enableScanFileDelay == right.enableScanFileDelay &&
        left.dateTimeFormat == right.dateTimeFormat;
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

LRESULT GeneralSettingsDialog::GeneralPageDialog::OnDateTimeFormatChanged(
    WORD notifyCode, WORD id, HWND control, BOOL& handled) {
    return owner_ ? owner_->OnDateTimeFormatChanged(notifyCode, id, control, handled) : 0;
}

LRESULT GeneralSettingsDialog::GeneralPageDialog::OnSettingChanged(
    WORD notifyCode, WORD id, HWND control, BOOL& handled) {
    return owner_ ? owner_->OnSettingChanged(notifyCode, id, control, handled) : 0;
}

LRESULT GeneralSettingsDialog::UserInterfacePageDialog::OnSettingChanged(
    WORD notifyCode, WORD id, HWND control, BOOL& handled) {
    return owner_ ? owner_->OnSettingChanged(notifyCode, id, control, handled) : 0;
}

LRESULT GeneralSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    initializing_ = true;
    SetWindowTextW(L"Settings");
    CreatePages();

    SendMessageW(Control(IDC_SHOW_STATUS_BAR), BM_SETCHECK,
        settings_.showStatusBar ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Control(IDC_SHOW_TOOLBAR), BM_SETCHECK,
        settings_.showToolbar ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(Control(IDC_ENABLE_SCAN_FILE_DELAY), BM_SETCHECK,
        settings_.enableScanFileDelay ? BST_CHECKED : BST_UNCHECKED, 0);

    const auto formatCombo = Control(IDC_DATE_TIME_FORMAT);
    SendMessageW(formatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kUseWindowsDefaultLabel));
    for (const auto* format : kCommonDateTimeFormats) {
        SendMessageW(formatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format));
    }
    const auto* visibleFormat = settings_.dateTimeFormat.empty()
        ? kUseWindowsDefaultLabel : settings_.dateTimeFormat.c_str();
    if (SendMessageW(formatCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1),
        reinterpret_cast<LPARAM>(visibleFormat)) == CB_ERR) {
        ::SetWindowTextW(Control(IDC_DATE_TIME_FORMAT), visibleFormat);
    }
    UpdateDateTimeFormatSample();

    const auto splitterPosition = std::format(L"{}", settings_.mainSplitterPosition);
    ::SetWindowTextW(Control(IDC_MAIN_SPLITTER_POSITION), splitterPosition.c_str());
    ::SetWindowTextW(Control(IDC_LAST_OPENED_CATALOG), settings_.lastCatalogPath.c_str());

    PopulateTree();
    SelectInitialPage();
    ShowPage(Page::General);
    ::SetFocus(GetDlgItem(IDC_SETTINGS_TREE));
    SetApplyEnabled(false);
    initializing_ = false;
    return FALSE;
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
        const auto formatCombo = Control(IDC_DATE_TIME_FORMAT);
        const auto selection = SendMessageW(formatCombo, CB_GETCURSEL, 0, 0);
        if (selection != CB_ERR) {
            const auto length = SendMessageW(formatCombo, CB_GETLBTEXTLEN, selection, 0);
            if (length != CB_ERR) {
                std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
                SendMessageW(formatCombo, CB_GETLBTEXT, selection, reinterpret_cast<LPARAM>(value.data()));
                value.resize(static_cast<std::size_t>(length));
                ::SetWindowTextW(Control(IDC_DATE_TIME_FORMAT), value.c_str());
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

HWND GeneralSettingsDialog::Control(int id) const {
    if (const auto control = GetDlgItem(id)) return control;
    if (generalPage_.m_hWnd) {
        if (const auto control = generalPage_.GetDlgItem(id)) return control;
    }
    if (userInterfacePage_.m_hWnd) {
        if (const auto control = userInterfacePage_.GetDlgItem(id)) return control;
    }
    return nullptr;
}

void GeneralSettingsDialog::CreatePages() {
    generalPage_.SetOwner(this);
    userInterfacePage_.SetOwner(this);
    generalPage_.Create(m_hWnd);
    userInterfacePage_.Create(m_hWnd);
    PositionPage(generalPage_.m_hWnd);
    PositionPage(userInterfacePage_.m_hWnd);
}

void GeneralSettingsDialog::PositionPage(HWND page) {
    RECT header{};
    ::GetWindowRect(GetDlgItem(IDC_SETTINGS_HEADER), &header);
    ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<POINT*>(&header), 2);

    RECT panel{};
    ::GetWindowRect(GetDlgItem(IDC_SETTINGS_PANEL), &panel);
    ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<POINT*>(&panel), 2);

    const int x = panel.left + 1;
    const int y = header.bottom + 2;
    const int width = panel.right - panel.left - 2;
    const int height = panel.bottom - y - 1;
    ::SetWindowPos(page, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
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
    ::ShowWindow(generalPage_.m_hWnd, showGeneral ? SW_SHOW : SW_HIDE);
    ::ShowWindow(userInterfacePage_.m_hWnd, showUserInterface ? SW_SHOW : SW_HIDE);
}

bool GeneralSettingsDialog::TryReadControls(wit::platform::AppSettings& settings, bool showValidation) {
    settings = settings_;
    settings.showStatusBar = SendMessageW(Control(IDC_SHOW_STATUS_BAR), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.showToolbar = SendMessageW(Control(IDC_SHOW_TOOLBAR), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.enableScanFileDelay = SendMessageW(Control(IDC_ENABLE_SCAN_FILE_DELAY), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.dateTimeFormat = SelectedDateTimeFormat();

    if (!wit::platform::IsValidDateTimeFormat(settings.dateTimeFormat)) {
        if (showValidation) {
            MessageBoxW(L"Use tokens like YYYY, MM, DD, HH, mm, and ss. Example: YYYY-MM-DD HH:mm:ss.",
                L"Date and Time Format", MB_OK | MB_ICONWARNING);
            ::SetFocus(Control(IDC_DATE_TIME_FORMAT));
        }
        return false;
    }
    return true;
}

bool GeneralSettingsDialog::ApplyChanges(bool closeAfterApply) {
    wit::platform::AppSettings updated;
    if (!TryReadControls(updated, true)) return false;
    if (!SameEditableSettings(updated, settings_)) {
        if (!applyHandler_ || applyHandler_(updated) != ApplyResult::Applied) return false;
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

std::wstring GeneralSettingsDialog::SelectedDateTimeFormat() const {
    auto value = ControlText(IDC_DATE_TIME_FORMAT);
    if (value == kUseWindowsDefaultLabel) return {};
    return value;
}

std::wstring GeneralSettingsDialog::ControlText(int id) const {
    const auto control = Control(id);
    const auto length = ::GetWindowTextLengthW(control);
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    ::GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(length));
    return value;
}

void GeneralSettingsDialog::UpdateDateTimeFormatSample() {
    const auto pattern = SelectedDateTimeFormat();
    const auto sample = wit::platform::IsValidDateTimeFormat(pattern)
        ? wit::platform::FormatDateTimeSample(pattern) : std::wstring(L"Invalid format");
    ::SetWindowTextW(Control(IDC_DATE_TIME_FORMAT_SAMPLE), sample.c_str());
}

}
