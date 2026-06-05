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

namespace wit::ui {
namespace {
void CopyText(std::wstring_view text, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    StringCchCopyNW(buffer, bufferSize, text.data(), text.size());
}
}

bool SearchDialog::Show(HWND owner, wit::search::ISearchRepository* search, std::function<void()> onClose) {
    if (!search) return false;
    launchOwner_ = owner;
    search_ = search;
    onClose_ = std::move(onClose);
    if (!m_hWnd && Create(nullptr) == nullptr) return false;
    ShowWindow(IsIconic() ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(m_hWnd);
    return true;
}

void SearchDialog::Close() {
    if (m_hWnd) DestroyWindow();
}

void SearchDialog::RefreshDisplay() {
    if (!m_hWnd || !results_) return;
    if (search_ && !nameTerm_.empty()) total_ = search_->CountByName(nameTerm_);
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

LRESULT SearchDialog::OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT SearchDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    results_ = nullptr;
    launchOwner_ = nullptr;
    search_ = nullptr;
    nameTerm_.clear();
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

void SearchDialog::Initialize() {
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

void SearchDialog::Search() {
    const int length = ::GetWindowTextLengthW(GetDlgItem(IDC_SEARCH_NAME));
    std::wstring term(static_cast<std::size_t>(length) + 1, L'\0');
    GetDlgItemTextW(IDC_SEARCH_NAME, term.data(), length + 1);
    term.resize(static_cast<std::size_t>(length));
    if (term.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        nameTerm_.clear();
        total_ = 0;
        ClearCache();
        ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
        SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Enter a name to search for.");
        return;
    }

    nameTerm_ = term;
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

void SearchDialog::ClearCache() {
    cacheClock_ = 0;
    cachedPages_.clear();
}

void SearchDialog::ResetResultItemCache() {
    if (!results_) return;
    ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
    ListView_SetItemCountEx(results_, total_, LVSICF_NOINVALIDATEALL);
}

void SearchDialog::CachePage(int pageStart) {
    if (!search_ || nameTerm_.empty() || pageStart < 0 || pageStart >= total_) return;

    const int normalizedStart = (pageStart / PageSize) * PageSize;
    const auto found = std::ranges::find_if(cachedPages_,
        [normalizedStart](const CachedPage& page) { return page.start == normalizedStart; });
    if (found != cachedPages_.end()) {
        found->lastUsed = ++cacheClock_;
        return;
    }

    CachedPage page;
    page.start = normalizedStart;
    page.items = search_->PageByName(nameTerm_, normalizedStart, PageSize);
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

std::wstring SearchDialog::TextFor(int row, int column) {
    wchar_t buffer[4096]{};
    TextFor(row, column, buffer, std::size(buffer));
    return buffer;
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

