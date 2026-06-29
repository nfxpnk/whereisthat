#include "wit_gui/SearchPane.h"
#include "wit_gui/FileListPane.h"
#include "wit_gui/BrowserItemIcons.h"
#include <wit_infra/PathHelpers.h>
#include <wit_infra/AppSettings.h>
#include "wit_infra/StringUtils.h"
#include <wit_infra/Win32Helpers.h>
#include <CommCtrl.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cwctype>
#include <format>
#include <iterator>
#include <optional>
#include <strsafe.h>
#include <string_view>
#include <wincodec.h>
#include <utility>
#include <windowsx.h>
#include <Shellapi.h>

namespace wit::ui {
namespace {
struct SearchColumnDefinition {
    const wchar_t* key;
    const wchar_t* name;
    int defaultWidth;
    int format;
};

constexpr int kMinimumColumnWidth = 20;
constexpr int kMaximumColumnWidth = 4000;
constexpr int MaxSelectedRowsForStatus = 256;
constexpr std::array<SearchColumnDefinition, 5> kSearchColumns{{
    {L"SearchResults.Name", L"File, Folder or Disk", 145, LVCFMT_LEFT},
    {L"SearchResults.Type", L"Type", 66, LVCFMT_LEFT},
    {L"SearchResults.Size", L"Size", 110, LVCFMT_RIGHT},
    {L"SearchResults.Path", L"Path", 170, LVCFMT_LEFT},
    {L"SearchResults.Modified", L"Modified", 105, LVCFMT_LEFT},
}};

bool IsValidColumnWidth(int width) {
    return width >= kMinimumColumnWidth && width <= kMaximumColumnWidth;
}

int SearchColumnWidth(const wit::platform::AppSettings& settings, const SearchColumnDefinition& column) {
    const auto saved = settings.searchListColumnWidths.find(column.key);
    return saved != settings.searchListColumnWidths.end() && IsValidColumnWidth(saved->second)
        ? saved->second : column.defaultWidth;
}
void CopyText(std::wstring_view text, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    StringCchCopyNW(buffer, bufferSize, text.data(), text.size());
}

std::optional<wit::core::FileSortColumn> SortColumnFromResultColumn(int column) {
    switch (column) {
    case 0: return wit::core::FileSortColumn::Name;
    case 1: return wit::core::FileSortColumn::Type;
    case 2: return wit::core::FileSortColumn::Size;
    case 3: return wit::core::FileSortColumn::Path;
    case 4: return wit::core::FileSortColumn::Modified;
    default: return std::nullopt;
    }
}

int ResultColumnFromSortColumn(wit::core::FileSortColumn column) {
    switch (column) {
    case wit::core::FileSortColumn::Type: return 1;
    case wit::core::FileSortColumn::Size: return 2;
    case wit::core::FileSortColumn::Path: return 3;
    case wit::core::FileSortColumn::Modified: return 4;
    case wit::core::FileSortColumn::Name:
    default: return 0;
    }
}

void UpdateListViewSortIndicators(HWND list, int sortColumn, bool ascending) {
    const HWND header = ListView_GetHeader(list);
    if (!header) return;
    const int count = Header_GetItemCount(header);
    for (int index = 0; index < count; ++index) {
        HDITEMW item{HDI_FORMAT};
        if (!Header_GetItem(header, index, &item)) continue;
        item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (index == sortColumn) item.fmt |= ascending ? HDF_SORTUP : HDF_SORTDOWN;
        Header_SetItem(header, index, &item);
    }
}

bool DirectoryExists(const std::wstring& path) {
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring QuoteExplorerPath(const std::wstring& path) {
    return L"\"" + path + L"\"";
}

void ShowFolderNotFound(HWND owner) {
    ::MessageBoxW(owner,
        L"Sorry, this folder does not exist any more on the media.\nPlease update data for this media.",
        L"Folder not found", MB_OK | MB_ICONWARNING);
}

void ShowFileNotFound(HWND owner) {
    ::MessageBoxW(owner,
        L"Sorry, this file does not exist any more on the media.\nPlease update data for this media.",
        L"File not found", MB_OK | MB_ICONWARNING);
}

bool FileExists(const std::wstring& path) {
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void OpenInExplorerOrAlert(HWND owner, const std::wstring& path, bool selectItem) {
    if (path.empty() || (selectItem ? !FileExists(path) : !DirectoryExists(path))) {
        if (selectItem) {
            ShowFileNotFound(owner);
        } else {
            ShowFolderNotFound(owner);
        }
        return;
    }
    const auto parameters = selectItem ? (L"/select," + QuoteExplorerPath(path)) : QuoteExplorerPath(path);
    const auto result = reinterpret_cast<INT_PTR>(
        ::ShellExecuteW(owner, L"open", L"explorer.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        if (selectItem) {
            ShowFileNotFound(owner);
        } else {
            ShowFolderNotFound(owner);
        }
    }
}
}

SearchDialog::SearchDialog() : searchReaper_([this]() { ReapWorkers(); }) {
}

SearchDialog::~SearchDialog() {
    CancelSearchLoad();
    CancelPageLoad();
    DrainWorkers();
    {
        std::scoped_lock lock(searchReaperMutex_);
        stopSearchReaper_ = true;
    }
    searchReaperCondition_.notify_one();
    if (searchReaper_.joinable()) searchReaper_.join();
}

bool SearchDialog::Show(HWND owner, wit::search::ISearchRepository* search, LocateResultHandler onLocate,
    std::function<void()> onClose) {
    if (!search) return false;
    const bool repositoryChanged = search_ && search_ != search;
    if (m_hWnd && repositoryChanged) {
        CancelSearchLoad();
        CancelPageLoad();
        DrainWorkers();
    }
    launchOwner_ = owner;
    search_ = search;
    onLocate_ = std::move(onLocate);
    onClose_ = std::move(onClose);
    if (!m_hWnd && Create(nullptr) == nullptr) return false;
    if (repositoryChanged) {
        total_ = 0;
        ClearCache();
        ResetResultItemCache();
        if ((resultMode_ == ResultMode::Quick && !nameTerm_.empty()) ||
            (resultMode_ == ResultMode::Advanced && !advancedExpression_.criteria.empty())) {
            BeginSearchLoad();
        }
    }
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
    if ((resultMode_ == ResultMode::Quick && !nameTerm_.empty()) ||
        (resultMode_ == ResultMode::Advanced && !advancedExpression_.criteria.empty())) {
        BeginSearchLoad();
    }
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

LRESULT SearchDialog::OnSize(UINT, WPARAM, LPARAM, BOOL& handled) {
    UpdateStatusParts();
    handled = FALSE;
    return 0;
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
    CancelSearchLoad();
    CancelPageLoad();
    SetDlgItemTextW(IDC_ADVANCED_SEARCH_QUERY, L"");
    advancedExpression_ = {};
    resultMode_ = ResultMode::Advanced;
    total_ = 0;
    ClearCache();
    ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
    elapsedSeconds_ = 0.0;
    UpdateStatusText();
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

LRESULT SearchDialog::OnOpenInExplorer(WORD, WORD, HWND, BOOL&) {
    const auto* entry = FocusedEntry();
    if (!entry) return 0;
    const bool selectItem = !entry->isDirectory || entry->isArchive;
    OpenInExplorerOrAlert(m_hWnd, wit::platform::Join(entry->parentPath, entry->name), selectItem);
    return 0;
}

LRESULT SearchDialog::OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT SearchDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    CancelSearchLoad();
    CancelPageLoad();
    DrainWorkers();
    (void)PersistColumnWidths();
    if (results_) {
        const HWND header = ListView_GetHeader(results_);
        if (header) RemoveWindowSubclass(header, HeaderSubclassProc, 1);
    }
    if (results_ && searchImages_) ListView_SetImageList(results_, nullptr, LVSIL_SMALL);
    const HWND searchName = GetDlgItem(IDC_SEARCH_NAME);
    if (searchName) RemoveWindowSubclass(searchName, SearchNameSubclassProc, 1);
    if (searchImages_) {
        ImageList_Destroy(searchImages_);
        searchImages_ = nullptr;
    }
    results_ = nullptr;
    status_ = nullptr;
    launchOwner_ = nullptr;
    search_ = nullptr;
    onLocate_ = {};
    nameTerm_.clear();
    advancedExpression_ = {};
    resultMode_ = ResultMode::Quick;
    total_ = 0;
    quickSearchHistory_.clear();
    quickSearchHistoryIndex_ = -1;
    quickSearchHistoryDraft_.clear();
    ClearCache();
    auto onClose = std::move(onClose_);
    onClose_ = {};
    if (onClose) onClose();
    return 0;
}

LRESULT SearchDialog::OnSearchComplete(UINT, WPARAM, LPARAM, BOOL&) {
    std::optional<AsyncSearchResult> result;
    const auto mailbox = searchMailbox_;
    if (mailbox) {
        std::scoped_lock lock(mailbox->mutex);
        if (mailbox->pendingResult && mailbox->pendingResult->requestId == searchRequestId_) {
            result = std::move(mailbox->pendingResult);
        }
        mailbox->pendingResult.reset();
    }
    if (!result || !results_) return 0;
    RetireWorker(searchWorker_);
    if (mailbox == searchMailbox_) searchMailbox_.reset();
    elapsedSeconds_ = result->elapsedSeconds;

    ClearCache();
    if (!result->error.empty()) {
        total_ = 0;
        ResetResultItemCache();
        SetDlgItemTextW(IDC_SEARCH_SUMMARY, result->error.c_str());
        UpdateStatusText();
        return 0;
    }

    total_ = result->total;
    if (!result->firstPage.empty()) {
        CachedPage page;
        page.start = 0;
        page.items = std::move(result->firstPage);
        page.lastUsed = ++cacheClock_;
        cachedPages_.push_back(std::move(page));
    }
    ResetResultItemCache();
    const auto summary = total_ == 0 ? std::wstring(L"No matching items found.") :
        std::format(L"{} matching item{}.", total_, total_ == 1 ? L"" : L"s");
    SetDlgItemTextW(IDC_SEARCH_SUMMARY, summary.c_str());
    UpdateStatusText();
    return 0;
}

LRESULT SearchDialog::OnPersistColumnWidths(UINT, WPARAM, LPARAM, BOOL&) {
    (void)PersistColumnWidths();
    return 0;
}

LRESULT SearchDialog::OnPageReady(UINT, WPARAM, LPARAM, BOOL&) {
    std::optional<AsyncPageResult> result;
    const auto mailbox = pageMailbox_;
    if (mailbox) {
        std::scoped_lock lock(mailbox->mutex);
        if (mailbox->pendingResult && mailbox->pendingResult->requestId == pageRequestId_) {
            result = std::move(mailbox->pendingResult);
        }
        mailbox->pendingResult.reset();
    }
    RetireWorker(pageWorker_);
    if (!result || !results_) return 0;
    if (mailbox == pageMailbox_) pageMailbox_.reset();
    if (!result->error.empty()) {
        SetDlgItemTextW(IDC_SEARCH_SUMMARY, result->error.c_str());
        return 0;
    }
    auto existing = std::ranges::find_if(cachedPages_,
        [start = result->page.start](const CachedPage& page) { return page.start == start; });
    result->page.lastUsed = ++cacheClock_;
    const int first = result->page.start;
    const int last = first + static_cast<int>(result->page.items.size()) - 1;
    if (existing != cachedPages_.end()) {
        *existing = std::move(result->page);
    } else {
        cachedPages_.push_back(std::move(result->page));
    }
    while (cachedPages_.size() > MaxCachedPages) {
        const auto oldest = std::ranges::min_element(cachedPages_,
            [](const CachedPage& left, const CachedPage& right) { return left.lastUsed < right.lastUsed; });
        if (oldest == cachedPages_.end()) break;
        cachedPages_.erase(oldest);
    }
    if (last >= first) ListView_RedrawItems(results_, first, last);
    UpdateStatusText();
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
    if (displayInfo->item.mask & LVIF_IMAGE) {
        const auto* entry = CachedEntryAt(displayInfo->item.iItem);
        if (!entry) SchedulePageLoad(displayInfo->item.iItem);
        displayInfo->item.iImage = entry ? ImageForBrowserEntry(*entry) : I_IMAGENONE;
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

LRESULT SearchDialog::OnColumnClick(int, LPNMHDR header, BOOL&) {
    const auto* click = reinterpret_cast<NMLISTVIEW*>(header);
    if (click) ToggleSortForColumn(click->iSubItem);
    return 0;
}

LRESULT SearchDialog::OnResultItemChanged(int, LPNMHDR header, BOOL&) {
    const auto* changed = reinterpret_cast<NMLISTVIEW*>(header);
    if (changed && (changed->uChanged & LVIF_STATE) &&
        ((changed->uOldState ^ changed->uNewState) & (LVIS_FOCUSED | LVIS_SELECTED))) {
        UpdateStatusText();
    }
    return 0;
}

LRESULT SearchDialog::OnHeaderWidthChanged(int, LPNMHDR header, BOOL& handled) {
    if (!header || !results_ || header->hwndFrom != ListView_GetHeader(results_)) {
        handled = FALSE;
        return 0;
    }
    PostMessageW(PersistColumnWidthsMessage, 0, 0);
    return 0;
}

LRESULT CALLBACK SearchDialog::HeaderSubclassProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
    UINT_PTR subclassId, DWORD_PTR referenceData) {
    auto* dialog = reinterpret_cast<SearchDialog*>(referenceData);
    if (message == WM_LBUTTONUP && dialog && dialog->m_hWnd) {
        dialog->PostMessageW(PersistColumnWidthsMessage, 0, 0);
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, HeaderSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

LRESULT CALLBACK SearchDialog::SearchNameSubclassProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
    UINT_PTR subclassId, DWORD_PTR referenceData) {
    auto* dialog = reinterpret_cast<SearchDialog*>(referenceData);
    if (message == WM_KEYDOWN && dialog && dialog->m_hWnd) {
        if (wparam == VK_UP) {
            dialog->NavigateQuickSearchHistory(1);
            return 0;
        }
        if (wparam == VK_DOWN) {
            dialog->NavigateQuickSearchHistory(-1);
            return 0;
        }
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, SearchNameSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wparam, lparam);
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
    status_ = GetDlgItem(IDC_SEARCH_STATUS);
    const HWND resultsHeader = ListView_GetHeader(results_);
    if (resultsHeader) SetWindowSubclass(resultsHeader, HeaderSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
    searchImages_ = CreateBrowserItemImageList();
    if (searchImages_) ListView_SetImageList(results_, searchImages_, LVSIL_SMALL);
    ListView_SetExtendedListViewStyle(results_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    const auto settings = wit::platform::LoadAppSettings();
    quickSearchHistory_ = settings.quickSearchHistory;
    if (const HWND searchName = GetDlgItem(IDC_SEARCH_NAME)) {
        SetWindowSubclass(searchName, SearchNameSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
    }
    for (std::size_t index = 0; index < kSearchColumns.size(); ++index) {
        const auto& definition = kSearchColumns[index];
        LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH | LVCF_FMT};
        column.fmt = definition.format;
        column.cx = SearchColumnWidth(settings, definition);
        column.pszText = const_cast<LPWSTR>(definition.name);
        ListView_InsertColumn(results_, static_cast<int>(index), &column);
    }
    UpdateSortIndicators();
    UpdateStatusParts();
    UpdateStatusText();

    SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Enter a name to search for. Use * to match any characters.");
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
    const int quickControls[] = {
        IDC_SEARCH_LABEL_NAME, IDC_SEARCH_NAME, IDC_SEARCH_EXECUTE, IDC_SEARCH_CASE_SENSITIVE
    };
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
        advanced ? L"Enter advanced search criteria." : L"Enter a name to search for. Use * to match any characters.");
}

void SearchDialog::Search() {
    const auto term = DialogText(IDC_SEARCH_NAME);
    if (term.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        CancelSearchLoad();
        CancelPageLoad();
        nameTerm_.clear();
        advancedExpression_ = {};
        resultMode_ = ResultMode::Quick;
        total_ = 0;
        ClearCache();
        ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
        elapsedSeconds_ = 0.0;
        UpdateStatusText();
        SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Enter a name to search for.");
        return;
    }

    nameTerm_ = term;
    caseSensitive_ = IsDlgButtonChecked(IDC_SEARCH_CASE_SENSITIVE) == BST_CHECKED;
    advancedExpression_ = {};
    resultMode_ = ResultMode::Quick;
    RememberQuickSearchQuery(term);
    BeginSearchLoad();
}

void SearchDialog::RememberQuickSearchQuery(const std::wstring& query) {
    auto settings = wit::platform::LoadAppSettings();
    wit::platform::RememberQuickSearchQuery(settings, query);
    if (wit::platform::SaveAppSettings(settings)) {
        quickSearchHistory_ = settings.quickSearchHistory;
        quickSearchHistoryIndex_ = -1;
        quickSearchHistoryDraft_.clear();
    }
}

bool SearchDialog::NavigateQuickSearchHistory(int direction) {
    if (quickSearchHistory_.empty()) return false;

    int next = quickSearchHistoryIndex_;
    if (next < 0) {
        if (direction < 0) return false;
        quickSearchHistoryDraft_ = DialogText(IDC_SEARCH_NAME);
        next = 0;
    } else {
        next += direction;
    }

    if (next < 0) {
        quickSearchHistoryIndex_ = -1;
        SetQuickSearchTextAtEnd(quickSearchHistoryDraft_);
        return true;
    }
    if (next >= static_cast<int>(quickSearchHistory_.size())) {
        next = static_cast<int>(quickSearchHistory_.size()) - 1;
    }

    quickSearchHistoryIndex_ = next;
    SetQuickSearchTextAtEnd(quickSearchHistory_[static_cast<std::size_t>(next)]);
    return true;
}

void SearchDialog::SetQuickSearchTextAtEnd(const std::wstring& text) {
    SetDlgItemTextW(IDC_SEARCH_NAME, text.c_str());
    const auto length = static_cast<WPARAM>(text.size());
    SendDlgItemMessageW(IDC_SEARCH_NAME, EM_SETSEL, length, static_cast<LPARAM>(length));
}

void SearchDialog::AdvancedSearch() {
    const auto query = DialogText(IDC_ADVANCED_SEARCH_QUERY);
    const auto parsed = wit::search::ParseAdvancedSearchQuery(query);
    if (!parsed.success) {
        CancelSearchLoad();
        CancelPageLoad();
        advancedExpression_ = {};
        resultMode_ = ResultMode::Advanced;
        total_ = 0;
        ClearCache();
        ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
        elapsedSeconds_ = 0.0;
        UpdateStatusText();
        SetDlgItemTextW(IDC_SEARCH_SUMMARY, parsed.error.c_str());
        return;
    }

    nameTerm_.clear();
    advancedExpression_ = parsed.expression;
    resultMode_ = ResultMode::Advanced;
    BeginSearchLoad();
}

void SearchDialog::BeginSearchLoad() {
    CancelSearchLoad();
    CancelPageLoad();
    if (!search_ || !results_) return;

    const auto requestId = ++searchRequestId_;
    const auto mode = resultMode_;
    const auto nameTerm = nameTerm_;
    const auto caseSensitive = caseSensitive_;
    const auto expression = advancedExpression_;
    const auto sort = sort_;
    auto* repository = search_;
    const HWND window = m_hWnd;
    const int pageSize = PageSize;
    auto mailbox = std::make_shared<AsyncSearchMailbox>();
    searchMailbox_ = mailbox;
    const std::weak_ptr<AsyncSearchMailbox> mailboxReference = mailbox;

    total_ = 0;
    elapsedSeconds_ = 0.0;
    ClearCache();
    ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
    UpdateStatusText();
    SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Searching...");

    searchWorker_ = std::jthread([window, requestId, mode, nameTerm, caseSensitive, expression, sort, repository,
        pageSize, mailboxReference](std::stop_token stopToken) {
        AsyncSearchResult result;
        result.requestId = requestId;
        const auto startedAt = std::chrono::steady_clock::now();
        auto prepared = mode == ResultMode::Quick
            ? repository->PrepareByName(nameTerm, pageSize, sort, caseSensitive)
            : repository->PrepareAdvanced(expression, pageSize, sort);
        if (stopToken.stop_requested()) return;
        result.total = prepared.total;
        result.firstPage = std::move(prepared.entries);
        result.elapsedSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - startedAt).count();

        result.error = repository->LastErrorMessage();
        if (result.error.empty() && result.total > 0 && result.firstPage.empty()) {
            result.error = L"Search results could not be loaded.";
        }
        PublishSearchResult(mailboxReference, window, std::move(result));
    });
}

void SearchDialog::CancelPageLoad() {
    ++pageRequestId_;
    pageMailbox_.reset();
    if (pageWorker_.joinable()) {
        pageWorker_.request_stop();
        if (search_) search_->CancelPending();
        RetireWorker(pageWorker_);
    }
}

void SearchDialog::CancelSearchLoad() {
    ++searchRequestId_;
    searchMailbox_.reset();
    if (searchWorker_.joinable()) {
        searchWorker_.request_stop();
        if (search_) search_->CancelPending();
        RetireWorker(searchWorker_);
    }
}

void SearchDialog::RetireWorker(std::jthread& worker) {
    if (!worker.joinable()) return;
    {
        std::scoped_lock lock(searchReaperMutex_);
        retiredSearchWorkers_.push_back(std::move(worker));
        ++activeRetiredSearchWorkers_;
    }
    searchReaperCondition_.notify_one();
}

void SearchDialog::DrainWorkers() {
    std::unique_lock lock(searchReaperMutex_);
    searchReaperCondition_.wait(lock, [this]() {
        return activeRetiredSearchWorkers_ == 0;
    });
}

void SearchDialog::ReapWorkers() {
    for (;;) {
        std::jthread retired;
        {
            std::unique_lock lock(searchReaperMutex_);
            searchReaperCondition_.wait(lock, [this]() {
                return stopSearchReaper_ || !retiredSearchWorkers_.empty();
            });
            if (retiredSearchWorkers_.empty()) {
                if (stopSearchReaper_) return;
                continue;
            }
            retired = std::move(retiredSearchWorkers_.back());
            retiredSearchWorkers_.pop_back();
        }
        if (retired.joinable()) retired.join();
        {
            std::scoped_lock lock(searchReaperMutex_);
            if (activeRetiredSearchWorkers_ > 0) --activeRetiredSearchWorkers_;
        }
        searchReaperCondition_.notify_all();
    }
}

void SearchDialog::PublishSearchResult(const std::weak_ptr<AsyncSearchMailbox>& mailbox, HWND window,
    AsyncSearchResult result) {
    const auto sharedMailbox = mailbox.lock();
    if (!sharedMailbox) return;
    {
        std::scoped_lock lock(sharedMailbox->mutex);
        sharedMailbox->pendingResult = std::move(result);
    }
    if (window) ::PostMessageW(window, SearchCompleteMessage, 0, 0);
}

void SearchDialog::PublishPageResult(const std::weak_ptr<AsyncPageMailbox>& mailbox, HWND window,
    AsyncPageResult result) {
    const auto sharedMailbox = mailbox.lock();
    if (!sharedMailbox) return;
    {
        std::scoped_lock lock(sharedMailbox->mutex);
        sharedMailbox->pendingResult = std::move(result);
    }
    if (window) ::PostMessageW(window, PageReadyMessage, 0, 0);
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
        ? search_->PageByName(nameTerm_, normalizedStart, PageSize, sort_, caseSensitive_)
        : search_->PageAdvanced(advancedExpression_, normalizedStart, PageSize, sort_);
    if (page.items.empty()) {
        const auto error = search_->LastErrorMessage();
        if (!error.empty()) SetDlgItemTextW(IDC_SEARCH_SUMMARY, error.c_str());
    }
    page.lastUsed = ++cacheClock_;
    cachedPages_.push_back(std::move(page));

    while (cachedPages_.size() > MaxCachedPages) {
        const auto oldest = std::ranges::min_element(cachedPages_,
            [](const CachedPage& left, const CachedPage& right) { return left.lastUsed < right.lastUsed; });
        if (oldest == cachedPages_.end()) break;
        cachedPages_.erase(oldest);
    }
}

void SearchDialog::SchedulePageLoad(int pageStart) {
    if (!search_ || pageStart < 0 || pageStart >= total_) return;
    if (resultMode_ == ResultMode::Quick && nameTerm_.empty()) return;
    if (resultMode_ == ResultMode::Advanced && advancedExpression_.criteria.empty()) return;

    const int normalizedStart = (pageStart / PageSize) * PageSize;
    const auto cached = std::ranges::find_if(cachedPages_,
        [normalizedStart](const CachedPage& page) { return page.start == normalizedStart; });
    if (cached != cachedPages_.end()) return;
    if (pageWorker_.joinable()) return;

    const auto requestId = ++pageRequestId_;
    const auto mode = resultMode_;
    const auto nameTerm = nameTerm_;
    const auto caseSensitive = caseSensitive_;
    const auto expression = advancedExpression_;
    const auto sort = sort_;
    auto* repository = search_;
    const HWND window = m_hWnd;
    auto mailbox = std::make_shared<AsyncPageMailbox>();
    pageMailbox_ = mailbox;
    const std::weak_ptr<AsyncPageMailbox> mailboxReference = mailbox;

    pageWorker_ = std::jthread([window, requestId, normalizedStart, mode, nameTerm, caseSensitive, expression, sort,
        repository, mailboxReference](std::stop_token stopToken) {
        AsyncPageResult result;
        result.requestId = requestId;
        result.page.start = normalizedStart;
        result.page.items = mode == ResultMode::Quick
            ? repository->PageByName(nameTerm, normalizedStart, PageSize, sort, caseSensitive)
            : repository->PageAdvanced(expression, normalizedStart, PageSize, sort);
        if (stopToken.stop_requested()) return;
        if (result.page.items.empty()) result.error = repository->LastErrorMessage();
        PublishPageResult(mailboxReference, window, std::move(result));
    });
}

void SearchDialog::PreloadRange(int firstRow, int lastRow) {
    if (total_ <= 0) return;
    firstRow = std::clamp(firstRow, 0, total_ - 1);
    lastRow = std::clamp(lastRow, firstRow, total_ - 1);

    const int firstPage = firstRow / PageSize;
    const int lastPage = lastRow / PageSize;
    for (int page = firstPage; page <= lastPage; ++page) {
        const int start = page * PageSize;
        const auto cached = std::ranges::find_if(cachedPages_,
            [start](const CachedPage& cachedPage) { return cachedPage.start == start; });
        if (cached == cachedPages_.end()) {
            SchedulePageLoad(start);
            return;
        }
    }
}

const wit::core::FileEntry* SearchDialog::CachedEntryAt(int row) {
    if (row < 0 || row >= total_) return nullptr;
    const int pageStart = (row / PageSize) * PageSize;
    const auto found = std::ranges::find_if(cachedPages_,
        [pageStart](const CachedPage& page) { return page.start == pageStart; });
    if (found == cachedPages_.end()) return nullptr;

    found->lastUsed = ++cacheClock_;
    const int index = row - found->start;
    return index >= 0 && index < static_cast<int>(found->items.size()) ? &found->items[index] : nullptr;
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

std::vector<wit::core::FileEntry> SearchDialog::SelectedEntriesInRange(int firstRow, int lastRow) {
    std::vector<wit::core::FileEntry> selected;
    if (!results_) return selected;
    firstRow = std::clamp(firstRow, 0, (std::max)(0, total_ - 1));
    lastRow = std::clamp(lastRow, firstRow, (std::max)(0, total_ - 1));
    for (int row = ListView_GetNextItem(results_, firstRow - 1, LVNI_SELECTED); row >= 0 && row <= lastRow;
        row = ListView_GetNextItem(results_, row, LVNI_SELECTED)) {
        if (const auto* entry = EntryAt(row)) selected.push_back(*entry);
    }
    return selected;
}

void SearchDialog::RestoreSelection(
    std::vector<wit::core::FileEntry> selectedEntries, std::int64_t focusedId, bool focusedIsDirectory) {
    if (!results_) return;
    const int topRow = (std::max)(0, ListView_GetTopIndex(results_));
    const int visibleRows = (std::max)(ListView_GetCountPerPage(results_), 1);
    const int firstRestoreRow = (std::max)(0, topRow - PageSize);
    const int lastRestoreRow = (std::min)(total_ - 1, topRow + visibleRows + PageSize);

    SendMessageW(results_, WM_SETREDRAW, FALSE, 0);
    ClearCache();
    ListView_SetItemState(results_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ResetResultItemCache();
    bool focusedRestored = focusedId == 0;
    for (int row = firstRestoreRow; row <= lastRestoreRow && (!selectedEntries.empty() || !focusedRestored); ++row) {
        const auto* entry = EntryAt(row);
        if (!entry) continue;
        const auto selected = std::ranges::find_if(selectedEntries, [entry](const auto& selectedEntry) {
            return selectedEntry.id == entry->id && selectedEntry.isDirectory == entry->isDirectory;
        });
        if (selected != selectedEntries.end()) {
            ListView_SetItemState(results_, row, LVIS_SELECTED, LVIS_SELECTED);
            selectedEntries.erase(selected);
        }
        if (!focusedRestored && entry->id == focusedId && entry->isDirectory == focusedIsDirectory) {
            ListView_SetItemState(results_, row, LVIS_FOCUSED, LVIS_FOCUSED);
            ListView_EnsureVisible(results_, row, FALSE);
            focusedRestored = true;
        }
    }
    SendMessageW(results_, WM_SETREDRAW, TRUE, 0);
    ::InvalidateRect(results_, nullptr, TRUE);
    ::UpdateWindow(results_);
}

void SearchDialog::ToggleSortForColumn(int column) {
    const auto sortColumn = SortColumnFromResultColumn(column);
    if (!sortColumn || !results_) return;

    if (sort_.column == *sortColumn) {
        sort_.ascending = !sort_.ascending;
    } else {
        sort_.column = *sortColumn;
        sort_.ascending = true;
    }
    UpdateSortIndicators();
    BeginSearchLoad();
}

void SearchDialog::UpdateSortIndicators() {
    if (!results_) return;
    UpdateListViewSortIndicators(results_, ResultColumnFromSortColumn(sort_.column), sort_.ascending);
}

void SearchDialog::UpdateStatusParts() {
    if (!status_ || !m_hWnd) return;
    RECT client{};
    ::GetClientRect(m_hWnd, &client);
    const int width = client.right - client.left;
    const int itemsEnd = (std::min)(160, width);
    const int timeWidth = 90;
    const int timeStart = (std::max)(itemsEnd, width - timeWidth);
    const int selectedWidth = (std::min)(250, (std::max)(0, timeStart - itemsEnd));
    const int selectedStart = (std::max)(itemsEnd, timeStart - selectedWidth);
    const int parts[] = {itemsEnd, selectedStart, timeStart, -1};
    SendMessageW(status_, SB_SETPARTS, static_cast<WPARAM>(std::size(parts)),
        reinterpret_cast<LPARAM>(parts));
}

void SearchDialog::UpdateStatusText() {
    if (!status_) return;
    const auto items = std::format(L"Items on list: {}", total_);
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(items.c_str()));

    const auto* focused = results_ ? CachedEntryAt(ListView_GetNextItem(results_, -1, LVNI_FOCUSED)) : nullptr;
    const auto focusedText = focused ? FileEntryStatusText(*focused) : std::wstring{};
    SendMessageW(status_, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(focusedText.c_str()));

    int selectedCount{};
    std::uint64_t selectedSize{};
    int sampledSelectedRows{};
    bool selectedSizeComplete = true;
    if (results_) {
        selectedCount = ListView_GetSelectedCount(results_);
        for (int row = ListView_GetNextItem(results_, -1, LVNI_SELECTED); row >= 0;
            row = ListView_GetNextItem(results_, row, LVNI_SELECTED)) {
            if (++sampledSelectedRows > MaxSelectedRowsForStatus) {
                selectedSizeComplete = false;
                break;
            }
            if (const auto* entry = CachedEntryAt(row)) selectedSize += entry->size;
            else selectedSizeComplete = false;
        }
    }
    const auto selectedText = selectedSizeComplete
        ? std::format(L"Selected items: {} (total {})", selectedCount, CompactFileSize(selectedSize))
        : std::format(L"Selected items: {}", selectedCount);
    SendMessageW(status_, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(selectedText.c_str()));

    const auto elapsedText = elapsedSeconds_ > 0.0
        ? std::format(L"{:.2f} s", elapsedSeconds_) : std::wstring{};
    SendMessageW(status_, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(elapsedText.c_str()));
}
bool SearchDialog::PersistColumnWidths() const {
    if (!results_) return false;
    auto settings = wit::platform::LoadAppSettings();
    for (std::size_t index = 0; index < kSearchColumns.size(); ++index) {
        const int width = ListView_GetColumnWidth(results_, static_cast<int>(index));
        if (IsValidColumnWidth(width)) {
            settings.searchListColumnWidths[kSearchColumns[index].key] = width;
        }
    }
    return wit::platform::SaveAppSettings(settings);
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
    ::AppendMenuW(menu, MF_STRING, ID_SEARCH_RESULTS_OPEN_EXPLORER_PLACEHOLDER, L"Open in Explorer");
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
    const auto* entry = CachedEntryAt(row);
    if (!entry) {
        SchedulePageLoad(row);
        return;
    }
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
        wit::core::FormatSizeRawBytesToBuffer(file.size, buffer, bufferSize);
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

