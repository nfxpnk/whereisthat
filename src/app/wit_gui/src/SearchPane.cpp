#include "wit_gui/SearchPane.h"
#include "wit_infra/StringUtils.h"
#include <wit_infra/Win32Helpers.h>
#include <CommCtrl.h>
#include <algorithm>
#include <format>
#include <iterator>
#include <strsafe.h>
#include <string_view>
#include <utility>
#include <windowsx.h>

namespace wit::ui {
namespace {
void CopyText(std::wstring_view text, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    StringCchCopyNW(buffer, bufferSize, text.data(), text.size());
}
}

bool SearchDialog::Show(HWND owner, wit::search::ISearchRepository* search, LocateResultHandler onLocate,
    std::function<void()> onClose) {
    if (!search) return false;
    launchOwner_ = owner;
    search_ = search;
    onLocate_ = std::move(onLocate);
    onClose_ = std::move(onClose);
    if (!m_hWnd && Create(nullptr) == nullptr) return false;
    ShowWindow(IsIconic() ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(m_hWnd);
    return true;
}

bool SearchDialog::Show(HWND owner, wit::search::ISearchRepository* search, std::function<void()> onClose) {
    return Show(owner, search, {}, std::move(onClose));
}

void SearchDialog::Close() {
    if (m_hWnd) DestroyWindow();
}

void SearchDialog::RefreshDisplay() {
    if (!m_hWnd || !results_) return;
    if (search_) {
        if (resultMode_ == ResultMode::Quick && !nameTerm_.empty()) {
            total_ = search_->CountByName(nameTerm_);
        } else if (resultMode_ == ResultMode::Advanced && !advancedExpression_.criteria.empty()) {
            total_ = search_->CountAdvanced(advancedExpression_);
        }
    }
    ClearCache();
    ResetResultItemCache();
    ::RedrawWindow(results_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

BOOL SearchDialog::PreTranslateMessage(MSG* message) {
    return m_hWnd != nullptr && IsDialogMessage(message);
}

LRESULT SearchDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    Initialize();
    DlgResize_Init(false, true);
    CenterWindow(launchOwner_);
    return TRUE;
}

LRESULT SearchDialog::OnExecuteSearch(WORD, WORD, HWND, BOOL&) {
    Search();
    return 0;
}

LRESULT SearchDialog::OnExecuteAdvancedSearch(WORD, WORD, HWND, BOOL&) {
    AdvancedSearch();
    return 0;
}

LRESULT SearchDialog::OnClearAdvancedSearch(WORD, WORD, HWND, BOOL&) {
    SetDlgItemTextW(IDC_ADVANCED_SEARCH_QUERY, L"");
    SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Enter advanced search criteria.");
    return 0;
}

LRESULT SearchDialog::OnContextMenu(UINT, WPARAM wparam, LPARAM lparam, BOOL& handled) {
    if (reinterpret_cast<HWND>(wparam) != results_) {
        handled = FALSE;
        return 0;
    }
    POINT screenPoint{};
    if (!PrepareContextMenuSelection(lparam, screenPoint)) return 0;
    ShowResultsContextMenu(screenPoint);
    return 0;
}

LRESULT SearchDialog::OnLocateInCatalog(WORD, WORD, HWND, BOOL&) {
    const auto* entry = FocusedEntry();
    const auto located = entry && onLocate_ ? onLocate_(wit::core::FileEntry(*entry)) : false;
    if (!located) {
        ::MessageBoxW(m_hWnd, L"The selected file could not be located in the catalog.",
            L"Locate in Catalog", MB_OK | MB_ICONINFORMATION);
    }
    return 0;
}

LRESULT SearchDialog::OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT SearchDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    results_ = nullptr;
    launchOwner_ = nullptr;
    search_ = nullptr;
    onLocate_ = {};
    nameTerm_.clear();
    advancedExpression_ = {};
    resultMode_ = ResultMode::Quick;
    total_ = 0;
    ClearCache();
    auto onClose = std::move(onClose_);
    onClose_ = {};
    if (onClose) onClose();
    return 0;
}

LRESULT SearchDialog::OnCloseCommand(WORD, WORD, HWND, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT SearchDialog::OnGetDisplayInfo(int, LPNMHDR header, BOOL&) {
    auto* displayInfo = reinterpret_cast<NMLVDISPINFOW*>(header);
    if (displayInfo->item.mask & LVIF_TEXT) {
        TextFor(displayInfo->item.iItem, displayInfo->item.iSubItem,
            displayInfo->item.pszText, displayInfo->item.cchTextMax);
    }
    displayInfo->item.mask |= LVIF_DI_SETITEM;
    return 0;
}

LRESULT SearchDialog::OnCacheHint(int, LPNMHDR header, BOOL&) {
    const auto* hint = reinterpret_cast<NMLVCACHEHINT*>(header);
    PreloadRange(hint->iFrom, hint->iTo);
    return 0;
}

LRESULT SearchDialog::OnTabChanged(int, LPNMHDR, BOOL&) {
    ShowTabPage(TabCtrl_GetCurSel(GetDlgItem(IDC_SEARCH_TABS)));
    return 0;
}

void SearchDialog::Initialize() {
    HWND tabs = GetDlgItem(IDC_SEARCH_TABS);
    TCITEMW item{TCIF_TEXT};
    item.pszText = const_cast<LPWSTR>(L"Quick Search");
    TabCtrl_InsertItem(tabs, 0, &item);
    item.pszText = const_cast<LPWSTR>(L"Advanced Search");
    TabCtrl_InsertItem(tabs, 1, &item);
    ShowTabPage(0);

    results_ = GetDlgItem(IDC_SEARCH_RESULTS);
    ListView_SetExtendedListViewStyle(results_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH | LVCF_FMT};
    column.fmt = LVCFMT_LEFT;
    column.cx = 145;
    column.pszText = const_cast<LPWSTR>(L"Name");
    ListView_InsertColumn(results_, 0, &column);
    column.cx = 66;
    column.pszText = const_cast<LPWSTR>(L"Type");
    ListView_InsertColumn(results_, 1, &column);
    column.fmt = LVCFMT_RIGHT;
    column.cx = 76;
    column.pszText = const_cast<LPWSTR>(L"Size");
    ListView_InsertColumn(results_, 2, &column);
    column.fmt = LVCFMT_LEFT;
    column.cx = 170;
    column.pszText = const_cast<LPWSTR>(L"Path");
    ListView_InsertColumn(results_, 3, &column);
    column.cx = 105;
    column.pszText = const_cast<LPWSTR>(L"Modified");
    ListView_InsertColumn(results_, 4, &column);

    SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Enter part of a file or folder name to search.");
}

std::wstring SearchDialog::DialogText(int controlId) const {
    const HWND control = GetDlgItem(controlId);
    const int length = ::GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetDlgItemTextW(controlId, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void SearchDialog::ShowTabPage(int index) {
    const bool advanced = index == 1;
    const int quickControls[] = {IDC_SEARCH_LABEL_NAME, IDC_SEARCH_NAME, IDC_SEARCH_EXECUTE};
    const int advancedControls[] = {
        IDC_ADVANCED_SEARCH_LABEL_CRITERIA,
        IDC_ADVANCED_SEARCH_QUERY,
        IDC_ADVANCED_SEARCH_EXECUTE,
        IDC_ADVANCED_SEARCH_CLEAR,
        IDC_ADVANCED_SEARCH_HELP
    };
    for (int control : quickControls) ::ShowWindow(GetDlgItem(control), advanced ? SW_HIDE : SW_SHOW);
    for (int control : advancedControls) ::ShowWindow(GetDlgItem(control), advanced ? SW_SHOW : SW_HIDE);
    SetDlgItemTextW(IDC_SEARCH_SUMMARY,
        advanced ? L"Enter advanced search criteria." : L"Enter part of a file or folder name to search.");
}

void SearchDialog::Search() {
    const auto term = DialogText(IDC_SEARCH_NAME);
    if (term.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        nameTerm_.clear();
        advancedExpression_ = {};
        resultMode_ = ResultMode::Quick;
        total_ = 0;
        ClearCache();
        ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
        SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Enter a name to search for.");
        return;
    }

    nameTerm_ = term;
    advancedExpression_ = {};
    resultMode_ = ResultMode::Quick;
    if (!search_) return;
    total_ = search_->CountByName(nameTerm_);
    ClearCache();
    ResetResultItemCache();
    if (total_ > 0) PreloadRange(0, (std::min)(total_ - 1, PageSize - 1));
    ::InvalidateRect(results_, nullptr, TRUE);
    const auto summary = total_ == 0 ? std::wstring(L"No matching items found.") :
        std::format(L"{} matching item{}.", total_, total_ == 1 ? L"" : L"s");
    SetDlgItemTextW(IDC_SEARCH_SUMMARY, summary.c_str());
}

void SearchDialog::AdvancedSearch() {
    const auto query = DialogText(IDC_ADVANCED_SEARCH_QUERY);
    const auto parsed = wit::search::ParseAdvancedSearchQuery(query);
    if (!parsed.success) {
        advancedExpression_ = {};
        resultMode_ = ResultMode::Advanced;
        total_ = 0;
        ClearCache();
        ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
        SetDlgItemTextW(IDC_SEARCH_SUMMARY, parsed.error.c_str());
        return;
    }

    nameTerm_.clear();
    advancedExpression_ = parsed.expression;
    resultMode_ = ResultMode::Advanced;
    if (!search_) return;
    total_ = search_->CountAdvanced(advancedExpression_);
    ClearCache();
    ResetResultItemCache();
    if (total_ > 0) PreloadRange(0, (std::min)(total_ - 1, PageSize - 1));
    ::InvalidateRect(results_, nullptr, TRUE);
    const auto summary = total_ == 0 ? std::wstring(L"No matching items found.") :
        std::format(L"{} matching item{}.", total_, total_ == 1 ? L"" : L"s");
    SetDlgItemTextW(IDC_SEARCH_SUMMARY, summary.c_str());
}

void SearchDialog::ClearCache() {
    cacheClock_ = 0;
    cachedPages_.clear();
}

void SearchDialog::ResetResultItemCache() {
    if (!results_) return;
    ListView_SetItemCountEx(results_, total_, LVSICF_NOSCROLL);
    ::InvalidateRect(results_, nullptr, TRUE);
}

void SearchDialog::CachePage(int pageStart) {
    if (!search_ || pageStart < 0 || pageStart >= total_) return;
    if (resultMode_ == ResultMode::Quick && nameTerm_.empty()) return;
    if (resultMode_ == ResultMode::Advanced && advancedExpression_.criteria.empty()) return;

    const int normalizedStart = (pageStart / PageSize) * PageSize;
    const auto found = std::ranges::find_if(cachedPages_,
        [normalizedStart](const CachedPage& page) { return page.start == normalizedStart; });
    if (found != cachedPages_.end()) {
        found->lastUsed = ++cacheClock_;
        return;
    }

    CachedPage page;
    page.start = normalizedStart;
    page.items = resultMode_ == ResultMode::Quick
        ? search_->PageByName(nameTerm_, normalizedStart, PageSize)
        : search_->PageAdvanced(advancedExpression_, normalizedStart, PageSize);
    page.lastUsed = ++cacheClock_;
    cachedPages_.push_back(std::move(page));

    while (cachedPages_.size() > MaxCachedPages) {
        const auto oldest = std::ranges::min_element(cachedPages_,
            [](const CachedPage& left, const CachedPage& right) { return left.lastUsed < right.lastUsed; });
        if (oldest == cachedPages_.end()) break;
        cachedPages_.erase(oldest);
    }
}

void SearchDialog::PreloadRange(int firstRow, int lastRow) {
    if (total_ <= 0) return;
    firstRow = std::clamp(firstRow, 0, total_ - 1);
    lastRow = std::clamp(lastRow, firstRow, total_ - 1);

    const int firstPage = (std::max)(0, (firstRow / PageSize) - 1);
    const int lastPage = (std::min)((total_ - 1) / PageSize, (lastRow / PageSize) + 1);
    for (int page = firstPage; page <= lastPage; ++page) {
        CachePage(page * PageSize);
    }
}

const wit::core::FileEntry* SearchDialog::EntryAt(int row) {
    if (row < 0 || row >= total_) return nullptr;

    CachePage(row);
    const int pageStart = (row / PageSize) * PageSize;
    const auto found = std::ranges::find_if(cachedPages_,
        [pageStart](const CachedPage& page) { return page.start == pageStart; });
    if (found == cachedPages_.end()) return nullptr;

    found->lastUsed = ++cacheClock_;
    const int index = row - found->start;
    return index >= 0 && index < static_cast<int>(found->items.size()) ? &found->items[index] : nullptr;
}

const wit::core::FileEntry* SearchDialog::FocusedEntry() {
    const int row = ListView_GetNextItem(results_, -1, LVNI_FOCUSED);
    return row >= 0 ? EntryAt(row) : nullptr;
}

bool SearchDialog::PrepareContextMenuSelection(LPARAM lparam, POINT& screenPoint) {
    if (!results_ || total_ <= 0) return false;
    const bool keyboardInvocation = lparam == -1;
    if (keyboardInvocation) {
        int row = ListView_GetNextItem(results_, -1, LVNI_FOCUSED);
        if (row < 0) row = ListView_GetNextItem(results_, -1, LVNI_SELECTED);
        if (row < 0 || !EntryAt(row)) return false;
        ListView_SetItemState(results_, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        RECT itemRect{};
        itemRect.left = LVIR_BOUNDS;
        if (ListView_GetItemRect(results_, row, &itemRect, LVIR_BOUNDS)) {
            screenPoint.x = itemRect.left + 16;
            screenPoint.y = itemRect.top + ((itemRect.bottom - itemRect.top) / 2);
            ::ClientToScreen(results_, &screenPoint);
        } else {
            ::GetWindowRect(results_, &itemRect);
            screenPoint.x = itemRect.left + 16;
            screenPoint.y = itemRect.top + 16;
        }
        return true;
    }

    screenPoint.x = GET_X_LPARAM(lparam);
    screenPoint.y = GET_Y_LPARAM(lparam);
    POINT clientPoint = screenPoint;
    ::ScreenToClient(results_, &clientPoint);
    LVHITTESTINFO hitTest{};
    hitTest.pt = clientPoint;
    const int row = ListView_HitTest(results_, &hitTest);
    if (row < 0 || (hitTest.flags & LVHT_ONITEM) == 0 || !EntryAt(row)) return false;
    ListView_SetItemState(results_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(results_, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    return true;
}

void SearchDialog::ShowResultsContextMenu(POINT screenPoint) {
    const auto menu = ::CreatePopupMenu();
    if (!menu) return;

    const auto fileManagement = ::CreatePopupMenu();
    const auto userList = ::CreatePopupMenu();
    const auto plugins = ::CreatePopupMenu();
    if (!fileManagement || !userList || !plugins) {
        if (fileManagement) ::DestroyMenu(fileManagement);
        if (userList) ::DestroyMenu(userList);
        if (plugins) ::DestroyMenu(plugins);
        ::DestroyMenu(menu);
        return;
    }

    constexpr UINT disabled = MF_STRING | MF_GRAYED;
    ::AppendMenuW(menu, MF_STRING, ID_SEARCH_RESULTS_LOCATE_IN_CATALOG, L"Locate in Catalog");
    ::AppendMenuW(menu, disabled, ID_SEARCH_RESULTS_VIEW_FILE_PLACEHOLDER, L"View File");
    ::AppendMenuW(menu, disabled, ID_SEARCH_RESULTS_LAUNCH_FILE_PLACEHOLDER, L"Launch File");
    ::AppendMenuW(menu, disabled, ID_SEARCH_RESULTS_OPEN_EXPLORER_PLACEHOLDER, L"Open in Explorer");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    ::AppendMenuW(fileManagement, disabled, ID_SEARCH_RESULTS_COPY_TO_PLACEHOLDER, L"Copy To...");
    ::AppendMenuW(fileManagement, disabled, ID_SEARCH_RESULTS_MOVE_TO_PLACEHOLDER, L"Move To...");
    ::AppendMenuW(fileManagement, disabled, ID_SEARCH_RESULTS_RENAME_PLACEHOLDER, L"Rename");
    ::AppendMenuW(fileManagement, disabled, ID_SEARCH_RESULTS_DELETE_PLACEHOLDER, L"Delete");
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileManagement), L"File Management");

    ::AppendMenuW(menu, disabled, ID_SEARCH_RESULTS_REMOVE_FROM_LIST_PLACEHOLDER, L"Remove from List");
    ::AppendMenuW(menu, disabled, ID_SEARCH_RESULTS_REMOVE_FROM_CATALOG_PLACEHOLDER, L"Remove from Catalog");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    ::AppendMenuW(userList, disabled, ID_SEARCH_RESULTS_ADD_TO_USER_LIST_PLACEHOLDER, L"Add to User List");
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(userList), L"User List");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    ::AppendMenuW(menu, disabled, ID_SEARCH_RESULTS_EDIT_DESCRIPTION_PLACEHOLDER, L"Edit Description");
    ::AppendMenuW(plugins, disabled, ID_SEARCH_RESULTS_NO_PLUGINS_PLACEHOLDER, L"(No plugins)");
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(plugins), L"Plugins");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, disabled, ID_SEARCH_RESULTS_PROPERTIES_PLACEHOLDER, L"Properties");

    ::SetForegroundWindow(m_hWnd);
    ::TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
        screenPoint.x, screenPoint.y, m_hWnd, nullptr);
    ::PostMessageW(m_hWnd, WM_NULL, 0, 0);
    ::DestroyMenu(menu);
}

void SearchDialog::TextFor(int row, int column, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    buffer[0] = L'\0';
    const auto* entry = EntryAt(row);
    if (!entry) return;
    const auto& file = *entry;
    switch (column) {
    case 0:
        CopyText(file.name, buffer, bufferSize);
        return;
    case 1:
        CopyText(file.isArchive ? std::wstring_view(L"Archive") :
            (file.isDirectory ? std::wstring_view(L"Folder") : std::wstring_view(file.extension)), buffer, bufferSize);
        return;
    case 2:
        if (!file.isDirectory) wit::core::FormatSizeToBuffer(file.size, buffer, bufferSize);
        return;
    case 3:
        CopyText(file.parentPath, buffer, bufferSize);
        return;
    case 4:
        wit::platform::FormatUnixTimestampToBuffer(file.modifiedAt, buffer, bufferSize);
        return;
    default:
        return;
    }
}
}

