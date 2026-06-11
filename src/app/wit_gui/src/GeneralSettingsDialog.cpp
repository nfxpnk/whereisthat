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

struct BoolGuard {
    explicit BoolGuard(bool& flag) : flag_(flag), previous_(flag) { flag_ = true; }
    ~BoolGuard() { flag_ = previous_; }
    BoolGuard(const BoolGuard&) = delete;
    BoolGuard& operator=(const BoolGuard&) = delete;

private:
    bool& flag_;
    bool previous_;
};

bool SameEditableSettings(const wit::platform::AppSettings& left, const wit::platform::AppSettings& right) {
    return left.showStatusBar == right.showStatusBar &&
        left.showToolbar == right.showToolbar &&
        left.enableScanFileDelay == right.enableScanFileDelay &&
        left.dateTimeFormat == right.dateTimeFormat;
}

const wchar_t* PageTitle(GeneralSettingsDialog::Page page) {
    switch (page) {
    case GeneralSettingsDialog::Page::General: return L"General Settings";
    case GeneralSettingsDialog::Page::UserInterface: return L"User Interface Setup";
    case GeneralSettingsDialog::Page::FileList: return L"File List Settings";
    case GeneralSettingsDialog::Page::DiskImage: return L"Disk Image Settings";
    case GeneralSettingsDialog::Page::Description: return L"Description Settings";
    }
    return L"General Settings";
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

bool GeneralSettingsDialog::Show(HWND owner, const wit::platform::AppSettings& current, Page initialPage,
    ApplyHandler applyHandler) {
    launchOwner_ = owner;
    initialPage_ = initialPage;
    applyHandler_ = std::move(applyHandler);
    if (m_hWnd) {
        LoadSettingsIntoControls(current, true);
        SelectPage(initialPage);
        ShowWindow(IsIconic() ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(m_hWnd);
        return true;
    }

    settings_ = current;
    if (Create(nullptr) == nullptr) return false;
    ShowWindow(SW_SHOW);
    SetForegroundWindow(m_hWnd);
    return true;
}

void GeneralSettingsDialog::Close() {
    if (m_hWnd) DestroyWindow();
}

BOOL GeneralSettingsDialog::PreTranslateMessage(MSG* message) {
    return m_hWnd != nullptr && IsDialogMessage(message);
}

LRESULT GeneralSettingsDialog::GeneralPageDialog::OnDateTimeFormatChanged(
    WORD notifyCode, WORD id, HWND control, BOOL& handled) {
    return Owner() ? Owner()->OnDateTimeFormatChanged(notifyCode, id, control, handled) : 0;
}

LRESULT GeneralSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    BoolGuard initGuard(initializing_);
    SetWindowTextW(L"Settings");
    CreatePages();

    const auto formatCombo = GeneralControl(IDC_DATE_TIME_FORMAT);
    SendMessageW(formatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kUseWindowsDefaultLabel));
    for (const auto* format : kCommonDateTimeFormats) {
        SendMessageW(formatCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format));
    }
    LoadSettingsIntoControls(settings_, false);

    PopulateTree();
    SelectPage(initialPage_);
    ::SetFocus(MainControl(IDC_SETTINGS_TREE));
    CenterWindow(launchOwner_);
    return FALSE;
}

LRESULT GeneralSettingsDialog::OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT GeneralSettingsDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    if (generalPage_.m_hWnd) generalPage_.DestroyWindow();
    if (userInterfacePage_.m_hWnd) userInterfacePage_.DestroyWindow();
    launchOwner_ = nullptr;
    applyHandler_ = {};
    initializing_ = false;
    initialPage_ = Page::General;
    return 0;
}

LRESULT GeneralSettingsDialog::OnTreeSelectionChanged(int, LPNMHDR header, BOOL&) {
    const auto* changed = reinterpret_cast<NMTREEVIEWW*>(header);
    ShowPage(static_cast<Page>(changed->itemNew.lParam));
    return 0;
}

LRESULT GeneralSettingsDialog::OnConfirm(WORD, WORD, HWND, BOOL&) {
    if (!ApplyChanges(true)) return 0;
    DestroyWindow();
    return 0;
}

LRESULT GeneralSettingsDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
    DestroyWindow();
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
        const auto formatCombo = GeneralControl(IDC_DATE_TIME_FORMAT);
        const auto selection = SendMessageW(formatCombo, CB_GETCURSEL, 0, 0);
        if (selection != CB_ERR) {
            const auto length = SendMessageW(formatCombo, CB_GETLBTEXTLEN, selection, 0);
            if (length != CB_ERR) {
                std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
                SendMessageW(formatCombo, CB_GETLBTEXT, selection, reinterpret_cast<LPARAM>(value.data()));
                value.resize(static_cast<std::size_t>(length));
                ::SetWindowTextW(formatCombo, value.c_str());
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

HWND GeneralSettingsDialog::MainControl(int id) const {
    return GetDlgItem(id);
}

HWND GeneralSettingsDialog::GeneralControl(int id) const {
    return generalPage_.m_hWnd ? generalPage_.GetDlgItem(id) : nullptr;
}

HWND GeneralSettingsDialog::UserInterfaceControl(int id) const {
    return userInterfacePage_.m_hWnd ? userInterfacePage_.GetDlgItem(id) : nullptr;
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
    ::GetWindowRect(MainControl(IDC_SETTINGS_HEADER), &header);
    ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<POINT*>(&header), 2);

    RECT panel{};
    ::GetWindowRect(MainControl(IDC_SETTINGS_PANEL), &panel);
    ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<POINT*>(&panel), 2);

    const int x = panel.left + 1;
    const int y = header.bottom + 2;
    const int width = panel.right - panel.left - 2;
    const int height = panel.bottom - y - 1;
    ::SetWindowPos(page, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void GeneralSettingsDialog::PopulateTree() {
    const auto tree = MainControl(IDC_SETTINGS_TREE);
    TreeView_DeleteAllItems(tree);
    InsertTreeItem(tree, PageTitle(Page::General), Page::General);
    InsertTreeItem(tree, PageTitle(Page::UserInterface), Page::UserInterface);
    InsertTreeItem(tree, PageTitle(Page::FileList), Page::FileList);
    InsertTreeItem(tree, PageTitle(Page::DiskImage), Page::DiskImage);
    InsertTreeItem(tree, PageTitle(Page::Description), Page::Description);
}

void GeneralSettingsDialog::SelectPage(Page page) {
    const auto tree = MainControl(IDC_SETTINGS_TREE);
    for (auto item = TreeView_GetRoot(tree); item; item = TreeView_GetNextSibling(tree, item)) {
        TVITEMW treeItem{};
        treeItem.mask = TVIF_PARAM;
        treeItem.hItem = item;
        TreeView_GetItem(tree, &treeItem);
        if (static_cast<Page>(treeItem.lParam) == page) {
            TreeView_SelectItem(tree, item);
            ShowPage(page);
            return;
        }
    }
    ShowPage(Page::General);
}

void GeneralSettingsDialog::ShowPage(Page page) {
    const bool showGeneral = page == Page::General;
    const bool showUserInterface = page == Page::UserInterface;

    SetDlgItemTextW(IDC_SETTINGS_HEADER_TITLE, PageTitle(page));
    ::ShowWindow(generalPage_.m_hWnd, showGeneral ? SW_SHOW : SW_HIDE);
    ::ShowWindow(userInterfacePage_.m_hWnd, showUserInterface ? SW_SHOW : SW_HIDE);
}

void GeneralSettingsDialog::LoadSettingsIntoControls(
    const wit::platform::AppSettings& settings, bool preservePendingEdits) {
    BoolGuard initGuard(initializing_);

    wit::platform::AppSettings currentControls;
    const bool haveControlSettings = preservePendingEdits;
    if (haveControlSettings) (void)TryReadControls(currentControls, false);

    if (!preservePendingEdits || !haveControlSettings || currentControls.showStatusBar == settings_.showStatusBar) {
        SendMessageW(UserInterfaceControl(IDC_SHOW_STATUS_BAR), BM_SETCHECK,
            settings.showStatusBar ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    if (!preservePendingEdits || !haveControlSettings || currentControls.showToolbar == settings_.showToolbar) {
        SendMessageW(UserInterfaceControl(IDC_SHOW_TOOLBAR), BM_SETCHECK,
            settings.showToolbar ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    if (!preservePendingEdits || !haveControlSettings ||
        currentControls.enableScanFileDelay == settings_.enableScanFileDelay) {
        SendMessageW(GeneralControl(IDC_ENABLE_SCAN_FILE_DELAY), BM_SETCHECK,
            settings.enableScanFileDelay ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    if (!preservePendingEdits || !haveControlSettings || currentControls.dateTimeFormat == settings_.dateTimeFormat) {
        SetDateTimeFormatControl(settings.dateTimeFormat);
    }

    const auto splitterPosition = std::format(L"{}", settings.mainSplitterPosition);
    ::SetWindowTextW(UserInterfaceControl(IDC_MAIN_SPLITTER_POSITION), splitterPosition.c_str());
    ::SetWindowTextW(GeneralControl(IDC_LAST_OPENED_CATALOG), settings.lastCatalogPath.c_str());

    settings_ = settings;
    UpdateDateTimeFormatSample();
    MarkDirtyIfChanged();
}

void GeneralSettingsDialog::SetDateTimeFormatControl(const std::wstring& format) {
    const auto formatCombo = GeneralControl(IDC_DATE_TIME_FORMAT);
    const auto* visibleFormat = format.empty() ? kUseWindowsDefaultLabel : format.c_str();
    if (SendMessageW(formatCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1),
        reinterpret_cast<LPARAM>(visibleFormat)) == CB_ERR) {
        ::SetWindowTextW(formatCombo, visibleFormat);
    }
}

bool GeneralSettingsDialog::TryReadControls(wit::platform::AppSettings& settings, bool showValidation) {
    settings = settings_;
    settings.showStatusBar = SendMessageW(UserInterfaceControl(IDC_SHOW_STATUS_BAR), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.showToolbar = SendMessageW(UserInterfaceControl(IDC_SHOW_TOOLBAR), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.enableScanFileDelay = SendMessageW(GeneralControl(IDC_ENABLE_SCAN_FILE_DELAY), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.dateTimeFormat = SelectedDateTimeFormat();

    if (!wit::platform::IsValidDateTimeFormat(settings.dateTimeFormat)) {
        if (showValidation) {
            MessageBoxW(L"Use tokens like YYYY, MM, DD, HH, mm, and ss. Example: YYYY-MM-DD HH:mm:ss.",
                L"Date and Time Format", MB_OK | MB_ICONWARNING);
            ::SetFocus(GeneralControl(IDC_DATE_TIME_FORMAT));
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
    if (!closeAfterApply) ::SetFocus(MainControl(IDC_SETTINGS_APPLY));
    return true;
}

void GeneralSettingsDialog::MarkDirtyIfChanged() {
    if (initializing_) return;
    wit::platform::AppSettings updated;
    const bool valid = TryReadControls(updated, false);
    SetApplyEnabled(!valid || !SameEditableSettings(updated, settings_));
}

void GeneralSettingsDialog::SetApplyEnabled(bool enabled) {
    ::EnableWindow(MainControl(IDC_SETTINGS_APPLY), enabled ? TRUE : FALSE);
}

std::wstring GeneralSettingsDialog::SelectedDateTimeFormat() const {
    auto value = ControlText(GeneralControl(IDC_DATE_TIME_FORMAT));
    if (value == kUseWindowsDefaultLabel) return {};
    return value;
}

std::wstring GeneralSettingsDialog::ControlText(HWND control) const {
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
    ::SetWindowTextW(GeneralControl(IDC_DATE_TIME_FORMAT_SAMPLE), sample.c_str());
}

}
