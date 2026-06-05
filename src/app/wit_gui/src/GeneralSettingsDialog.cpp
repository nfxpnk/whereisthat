#include "wit_gui/GeneralSettingsDialog.h"

#include <CommCtrl.h>
#include <array>
#include <cwchar>
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

constexpr int kNavWidth = 216;
constexpr int kMargin = 8;
constexpr int kGap = 8;
constexpr int kHeaderHeight = 31;
constexpr int kButtonWidth = 75;
constexpr int kButtonHeight = 23;
constexpr int kButtonGap = 8;
constexpr int kBottomHeight = 36;

bool SameSettings(const wit::platform::AppSettings& left, const wit::platform::AppSettings& right) {
    return left.showStatusBar == right.showStatusBar &&
        left.showToolbar == right.showToolbar &&
        left.enableScanFileDelay == right.enableScanFileDelay &&
        left.mainSplitterPosition == right.mainSplitterPosition &&
        left.dateTimeFormat == right.dateTimeFormat &&
        left.lastCatalogPath == right.lastCatalogPath;
}

void SetVisible(HWND dialog, int id, bool visible) {
    if (const auto control = GetDlgItem(dialog, id)) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

void MoveDlgItem(HWND dialog, int id, int x, int y, int width, int height) {
    if (const auto control = GetDlgItem(dialog, id)) {
        MoveWindow(control, x, y, width, height, TRUE);
    }
}

void BringDlgItemToTop(HWND dialog, int id) {
    if (const auto control = GetDlgItem(dialog, id)) {
        SetWindowPos(control, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void InsertTreeItem(HWND tree, const wchar_t* text, GeneralSettingsDialog::Page page) {
    TVINSERTSTRUCTW item{};
    item.hParent = TVI_ROOT;
    item.hInsertAfter = TVI_LAST;
    item.item.mask = TVIF_TEXT | TVIF_PARAM;
    item.item.pszText = const_cast<wchar_t*>(text);
    item.item.lParam = static_cast<LPARAM>(page);
    TreeView_InsertItem(tree, &item);
}

}

bool GeneralSettingsDialog::Show(HWND owner, const wit::platform::AppSettings& current,
    wit::platform::AppSettings& accepted, ApplyHandler applyHandler) {
    settings_ = current;
    applyHandler_ = std::move(applyHandler);
    accepted_ = false;
    if (DoModal(owner) != IDOK) return false;
    accepted = settings_;
    return accepted_;
}

LRESULT GeneralSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    initializing_ = true;
    SetWindowTextW(L"Settings");
    headerBrush_ = CreateSolidBrush(RGB(226, 234, 246));
    SendMessageW(WM_SETICON, ICON_SMALL,
        reinterpret_cast<LPARAM>(LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON))));
    SendMessageW(WM_SETICON, ICON_BIG,
        reinterpret_cast<LPARAM>(LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON))));
    SendDlgItemMessageW(IDC_SETTINGS_HEADER_ICON, STM_SETICON,
        reinterpret_cast<WPARAM>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON),
            IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED)), 0);
    ::ShowWindow(GetDlgItem(IDC_SETTINGS_PANEL), SW_HIDE);
    ::ShowWindow(GetDlgItem(IDC_SETTINGS_HEADER), SW_HIDE);

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
    RECT windowRect{};
    GetWindowRect(&windowRect);
    minimumTrackSize_.x = windowRect.right - windowRect.left;
    minimumTrackSize_.y = windowRect.bottom - windowRect.top;
    LayoutControls();
    SelectInitialPage();
    SetApplyEnabled(false);
    initializing_ = false;
    return TRUE;
}

LRESULT GeneralSettingsDialog::OnSize(UINT, WPARAM wparam, LPARAM, BOOL&) {
    if (wparam != SIZE_MINIMIZED) LayoutControls();
    return 0;
}

LRESULT GeneralSettingsDialog::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lparam, BOOL&) {
    auto* minMax = reinterpret_cast<MINMAXINFO*>(lparam);
    minMax->ptMinTrackSize.x = minimumTrackSize_.x;
    minMax->ptMinTrackSize.y = minimumTrackSize_.y;
    return 0;
}

LRESULT GeneralSettingsDialog::OnPaint(UINT, WPARAM, LPARAM, BOOL&) {
    PAINTSTRUCT paint{};
    const auto dc = BeginPaint(&paint);
    DrawPanelChrome(dc);
    EndPaint(&paint);
    return 0;
}

LRESULT GeneralSettingsDialog::OnControlColorStatic(UINT, WPARAM wparam, LPARAM lparam, BOOL& handled) {
    const auto control = reinterpret_cast<HWND>(lparam);
    if (control == GetDlgItem(IDC_SETTINGS_HEADER) || control == GetDlgItem(IDC_SETTINGS_HEADER_TITLE) ||
        control == GetDlgItem(IDC_SETTINGS_HEADER_ICON)) {
        const auto dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, RGB(226, 234, 246));
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        return reinterpret_cast<LRESULT>(headerBrush_);
    }
    handled = FALSE;
    return 0;
}

LRESULT GeneralSettingsDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
    if (headerBrush_) {
        DeleteObject(headerBrush_);
        headerBrush_ = nullptr;
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
    accepted_ = true;
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

    SetVisible(m_hWnd, IDC_SETTINGS_GROUP_DATE_TIME, showGeneral);
    SetVisible(m_hWnd, IDC_SETTINGS_LABEL_DATE_TIME, showGeneral);
    SetVisible(m_hWnd, IDC_DATE_TIME_FORMAT, showGeneral);
    SetVisible(m_hWnd, IDC_DATE_TIME_FORMAT_SAMPLE, showGeneral);
    SetVisible(m_hWnd, IDC_SETTINGS_GROUP_CATALOG, showGeneral);
    SetVisible(m_hWnd, IDC_SETTINGS_LABEL_LAST_CATALOG, showGeneral);
    SetVisible(m_hWnd, IDC_LAST_OPENED_CATALOG, showGeneral);
    SetVisible(m_hWnd, IDC_SETTINGS_GROUP_DEBUG, showGeneral);
    SetVisible(m_hWnd, IDC_ENABLE_SCAN_FILE_DELAY, showGeneral);
    SetVisible(m_hWnd, IDC_SETTINGS_GROUP_DISPLAY, showUserInterface);
    SetVisible(m_hWnd, IDC_SHOW_STATUS_BAR, showUserInterface);
    SetVisible(m_hWnd, IDC_SHOW_TOOLBAR, showUserInterface);
    SetVisible(m_hWnd, IDC_SETTINGS_GROUP_LAYOUT, showUserInterface);
    SetVisible(m_hWnd, IDC_SETTINGS_LABEL_SPLITTER, showUserInterface);
    SetVisible(m_hWnd, IDC_MAIN_SPLITTER_POSITION, showUserInterface);
}

void GeneralSettingsDialog::LayoutControls() {
    RECT client{};
    GetClientRect(&client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int bodyBottom = (std::max)(kMargin, height - kBottomHeight);
    const int panelLeft = kMargin + kNavWidth + kGap;
    const int panelWidth = (std::max)(0, width - panelLeft - kMargin);
    const int panelHeight = (std::max)(0, bodyBottom - kMargin);

    MoveDlgItem(m_hWnd, IDC_SETTINGS_TREE, kMargin, kMargin, kNavWidth, panelHeight);
    MoveDlgItem(m_hWnd, IDC_SETTINGS_HEADER_ICON, panelLeft + 10, kMargin + 8, 16, 16);
    MoveDlgItem(m_hWnd, IDC_SETTINGS_HEADER_TITLE, panelLeft + 34, kMargin + 11, (std::max)(0, panelWidth - 44), 16);

    const int contentLeft = panelLeft + 12;
    const int contentTop = kMargin + kHeaderHeight + 14;
    const int groupWidth = (std::max)(100, panelWidth - 24);
    const int labelLeft = contentLeft + 12;
    const int fieldLeft = contentLeft + 124;
    const int fieldRight = panelLeft + panelWidth - 20;
    const int fieldWidth = (std::max)(80, fieldRight - fieldLeft);

    MoveDlgItem(m_hWnd, IDC_SETTINGS_GROUP_DATE_TIME, contentLeft, contentTop, groupWidth, 60);
    MoveDlgItem(m_hWnd, IDC_SETTINGS_LABEL_DATE_TIME, labelLeft, contentTop + 23, 108, 16);
    MoveDlgItem(m_hWnd, IDC_DATE_TIME_FORMAT, fieldLeft, contentTop + 19, 164, 120);
    MoveDlgItem(m_hWnd, IDC_DATE_TIME_FORMAT_SAMPLE, fieldLeft + 174, contentTop + 23,
        (std::max)(60, fieldRight - fieldLeft - 174), 16);

    const int catalogTop = contentTop + 72;
    MoveDlgItem(m_hWnd, IDC_SETTINGS_GROUP_CATALOG, contentLeft, catalogTop, groupWidth, 58);
    MoveDlgItem(m_hWnd, IDC_SETTINGS_LABEL_LAST_CATALOG, labelLeft, catalogTop + 24, 108, 16);
    MoveDlgItem(m_hWnd, IDC_LAST_OPENED_CATALOG, fieldLeft, catalogTop + 20, fieldWidth, 23);
    const int debugTop = catalogTop + 70;
    MoveDlgItem(m_hWnd, IDC_SETTINGS_GROUP_DEBUG, contentLeft, debugTop, groupWidth, 46);
    MoveDlgItem(m_hWnd, IDC_ENABLE_SCAN_FILE_DELAY, labelLeft, debugTop + 23, 180, 16);

    MoveDlgItem(m_hWnd, IDC_SETTINGS_GROUP_DISPLAY, contentLeft, contentTop, groupWidth, 64);
    MoveDlgItem(m_hWnd, IDC_SHOW_STATUS_BAR, labelLeft, contentTop + 24, 150, 16);
    MoveDlgItem(m_hWnd, IDC_SHOW_TOOLBAR, labelLeft, contentTop + 44, 150, 16);

    const int layoutTop = contentTop + 76;
    MoveDlgItem(m_hWnd, IDC_SETTINGS_GROUP_LAYOUT, contentLeft, layoutTop, groupWidth, 58);
    MoveDlgItem(m_hWnd, IDC_SETTINGS_LABEL_SPLITTER, labelLeft, layoutTop + 24, 108, 16);
    MoveDlgItem(m_hWnd, IDC_MAIN_SPLITTER_POSITION, fieldLeft, layoutTop + 20, 74, 23);

    const int buttonsTop = height - kMargin - kButtonHeight;
    int buttonLeft = width - kMargin - (kButtonWidth * 4) - (kButtonGap * 3);
    MoveDlgItem(m_hWnd, IDOK, buttonLeft, buttonsTop, kButtonWidth, kButtonHeight);
    buttonLeft += kButtonWidth + kButtonGap;
    MoveDlgItem(m_hWnd, IDCANCEL, buttonLeft, buttonsTop, kButtonWidth, kButtonHeight);
    buttonLeft += kButtonWidth + kButtonGap;
    MoveDlgItem(m_hWnd, IDC_SETTINGS_APPLY, buttonLeft, buttonsTop, kButtonWidth, kButtonHeight);
    buttonLeft += kButtonWidth + kButtonGap;
    MoveDlgItem(m_hWnd, IDHELP, buttonLeft, buttonsTop, kButtonWidth, kButtonHeight);

    for (const auto id : {
        IDC_SETTINGS_TREE,
        IDC_DATE_TIME_FORMAT,
        IDC_LAST_OPENED_CATALOG,
        IDC_ENABLE_SCAN_FILE_DELAY,
        IDC_SHOW_STATUS_BAR,
        IDC_SHOW_TOOLBAR,
        IDC_MAIN_SPLITTER_POSITION,
        IDOK,
        IDCANCEL,
        IDC_SETTINGS_APPLY,
        IDHELP
    }) {
        BringDlgItemToTop(m_hWnd, id);
    }
    InvalidateRect(nullptr, TRUE);
}

void GeneralSettingsDialog::DrawPanelChrome(HDC dc) {
    RECT client{};
    GetClientRect(&client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int bodyBottom = (std::max)(kMargin, height - kBottomHeight);
    const int panelLeft = kMargin + kNavWidth + kGap;
    const int panelWidth = (std::max)(0, width - panelLeft - kMargin);
    const int panelHeight = (std::max)(0, bodyBottom - kMargin);
    if (panelWidth <= 0 || panelHeight <= 0) return;

    RECT panel{panelLeft, kMargin, panelLeft + panelWidth, kMargin + panelHeight};
    FillRect(dc, &panel, GetSysColorBrush(COLOR_BTNFACE));
    FrameRect(dc, &panel, GetSysColorBrush(COLOR_WINDOWFRAME));

    RECT header{panel.left + 1, panel.top + 1, panel.right - 1, panel.top + 1 + kHeaderHeight};
    FillRect(dc, &header, headerBrush_);
}

bool GeneralSettingsDialog::TryReadControls(wit::platform::AppSettings& settings, bool showValidation) {
    settings = settings_;
    settings.showStatusBar = IsDlgButtonChecked(IDC_SHOW_STATUS_BAR) == BST_CHECKED;
    settings.showToolbar = IsDlgButtonChecked(IDC_SHOW_TOOLBAR) == BST_CHECKED;
    settings.enableScanFileDelay = IsDlgButtonChecked(IDC_ENABLE_SCAN_FILE_DELAY) == BST_CHECKED;
    settings.dateTimeFormat = SelectedDateTimeFormat();
    settings.lastCatalogPath = ControlText(IDC_LAST_OPENED_CATALOG);

    if (!wit::platform::IsValidDateTimeFormat(settings.dateTimeFormat)) {
        if (showValidation) {
            MessageBoxW(L"Use tokens like YYYY, MM, DD, HH, mm, and ss. Example: YYYY-MM-DD HH:mm:ss.",
                L"Date and Time Format", MB_OK | MB_ICONWARNING);
            ::SetFocus(GetDlgItem(IDC_DATE_TIME_FORMAT));
        }
        return false;
    }

    const auto splitterText = ControlText(IDC_MAIN_SPLITTER_POSITION);
    wchar_t* parseEnd{};
    const auto splitterPosition = std::wcstol(splitterText.c_str(), &parseEnd, 10);
    if (splitterText.empty() || *parseEnd != L'\0') {
        if (showValidation) {
            MessageBoxW(L"Main splitter position must be an integer.", L"Layout",
                MB_OK | MB_ICONWARNING);
            ::SetFocus(GetDlgItem(IDC_MAIN_SPLITTER_POSITION));
        }
        return false;
    }
    settings.mainSplitterPosition = static_cast<int>(splitterPosition);
    return true;
}

bool GeneralSettingsDialog::ApplyChanges(bool closeAfterApply) {
    wit::platform::AppSettings updated;
    if (!TryReadControls(updated, true)) return false;
    if (!SameSettings(updated, settings_)) {
        const bool saved = applyHandler_ ? applyHandler_(updated) : wit::platform::SaveAppSettings(updated);
        if (!saved) {
            MessageBoxW(L"Unable to save settings.ini.", L"Settings", MB_OK | MB_ICONERROR);
            return false;
        }
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
    SetApplyEnabled(!valid || !SameSettings(updated, settings_));
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
