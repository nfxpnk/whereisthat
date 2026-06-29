#include "wit_gui/FileListPane.h"
#include "wit_gui/BrowserItemIcons.h"
#include "wit_infra/StringUtils.h"
#include <wit_infra/AppSettings.h>
#include <wit_infra/Win32Helpers.h>
#include <CommCtrl.h>
#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <optional>
#include <strsafe.h>
#include <string_view>
#include <utility>

namespace wit::ui {
namespace {
struct ColumnDefinition {
    const wchar_t* key;
    const wchar_t* name;
    int defaultWidth;
    int format;
};

constexpr int kMinimumColumnWidth = 20;
constexpr int kMaximumColumnWidth = 4000;

constexpr std::array<ColumnDefinition, 9> kBrowserRootColumns{{
    { L"BrowserRoot.Name", L"Name", 180, LVCFMT_LEFT },
    { L"BrowserRoot.MediaType", L"Media Type", 100, LVCFMT_LEFT },
    { L"BrowserRoot.Capacity", L"Capacity", 100, LVCFMT_RIGHT },
    { L"BrowserRoot.FreeSpace", L"Free Space", 100, LVCFMT_RIGHT },
    { L"BrowserRoot.LastUpdated", L"Last Updated", 165, LVCFMT_LEFT },
    { L"BrowserRoot.DiskNumber", L"Disk # / Count", 95, LVCFMT_RIGHT },
    { L"BrowserRoot.Description", L"Description", 180, LVCFMT_LEFT },
    { L"BrowserRoot.Category", L"Category", 120, LVCFMT_LEFT },
    { L"BrowserRoot.DiskLocation", L"Disk Location", 240, LVCFMT_LEFT },
}};

constexpr std::array<ColumnDefinition, 5> kBrowserContentColumns{{
    { L"BrowserContent.Name", L"Name", 200, LVCFMT_LEFT },
    { L"BrowserContent.Type", L"Type", 80, LVCFMT_LEFT },
    { L"BrowserContent.Size", L"Size", 130, LVCFMT_RIGHT },
    { L"BrowserContent.Path", L"Path", 320, LVCFMT_LEFT },
    { L"BrowserContent.Modified", L"Modified", 180, LVCFMT_LEFT },
}};

const wchar_t* DiskTypeLabel(wit::core::DiskType type) {
    switch (type) {
    case wit::core::DiskType::CD: return L"CD/DVD";
    case wit::core::DiskType::DVD: return L"DVD";
    case wit::core::DiskType::BluRay: return L"BluRay";
    case wit::core::DiskType::HardDisk: return L"HardDisk";
    case wit::core::DiskType::SolidStateDisk: return L"SolidStateDisk";
    case wit::core::DiskType::RemovableUSB: return L"RemovableUSB";
    case wit::core::DiskType::VirtualDisk: return L"VirtualDisk";
    default: return L"Other";
    }
}

void InsertColumn(HWND hwnd, int index, const wchar_t* name, int width, int format = LVCFMT_LEFT) {
    LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH | LVCF_FMT};
    column.fmt = format;
    column.cx = width;
    column.pszText = const_cast<LPWSTR>(name);
    ListView_InsertColumn(hwnd, index, &column);
}

bool IsValidColumnWidth(int width) {
    return width >= kMinimumColumnWidth && width <= kMaximumColumnWidth;
}

int WidthForColumn(const wit::platform::AppSettings& settings, const ColumnDefinition& column) {
    const auto saved = settings.fileListColumnWidths.find(column.key);
    if (saved == settings.fileListColumnWidths.end() || !IsValidColumnWidth(saved->second)) {
        return column.defaultWidth;
    }
    return saved->second;
}

template <std::size_t Size>
void InsertColumns(HWND hwnd, const std::array<ColumnDefinition, Size>& columns,
    const wit::platform::AppSettings& settings) {
    for (std::size_t index = 0; index < columns.size(); ++index) {
        const auto& column = columns[index];
        InsertColumn(hwnd, static_cast<int>(index), column.name, WidthForColumn(settings, column), column.format);
    }
}

void CopyText(std::wstring_view text, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    StringCchCopyNW(buffer, bufferSize, text.data(), text.size());
}

std::optional<wit::core::FileSortColumn> SortColumnFromContentColumn(int column) {
    switch (column) {
    case 0: return wit::core::FileSortColumn::Name;
    case 1: return wit::core::FileSortColumn::Type;
    case 2: return wit::core::FileSortColumn::Size;
    case 3: return wit::core::FileSortColumn::Path;
    case 4: return wit::core::FileSortColumn::Modified;
    default: return std::nullopt;
    }
}

bool IsBrowserRootColumn(int column) {
    return column >= 0 && column < static_cast<int>(kBrowserRootColumns.size());
}

wit::core::FileSortColumn SortColumnFromSettings(int column) {
    return SortColumnFromContentColumn(column).value_or(wit::core::FileSortColumn::Name);
}

int ContentColumnFromSortColumn(wit::core::FileSortColumn column) {
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
}

std::wstring CompactFileSize(std::uint64_t bytes) {
    auto result = wit::core::FormatSize(bytes);
    const auto decimal = result.find(L'.');
    const auto space = result.find(L' ');
    if (decimal != std::wstring::npos && space != std::wstring::npos) {
        auto end = space;
        while (end > decimal + 1 && result[end - 1] == L'0') result.erase(--end, 1);
        if (end == decimal + 1) result.erase(decimal, 1);
    }
    return result;
}

std::wstring FileEntryStatusText(const wit::core::FileEntry& entry) {
    auto text = entry.name + L", " + CompactFileSize(entry.size);
    const auto modifiedAt = wit::platform::FormatUnixDate(entry.modifiedAt);
    if (!modifiedAt.empty()) text += L", " + modifiedAt;
    return text;
}
FileListView::FileListView() : reaper_([this]() { ReapWorkers(); }) {
}

FileListView::~FileListView() {
    CancelLocationLoad();
    CancelPageLoad();
    DrainWorkers();
    {
        std::scoped_lock lock(reaperMutex_);
        stopReaper_ = true;
    }
    reaperCondition_.notify_one();
    if (reaper_.joinable()) reaper_.join();
    if (hwnd) RemoveWindowSubclass(hwnd, ListSubclassProc, SubclassId);
}
void FileListView::Attach(HWND handle) {
    if (hwnd && hwnd != handle) RemoveWindowSubclass(hwnd, ListSubclassProc, SubclassId);
    hwnd = handle;
    if (hwnd) SetWindowSubclass(hwnd, ListSubclassProc, SubclassId, reinterpret_cast<DWORD_PTR>(this));
    const auto settings = wit::platform::LoadAppSettings();
    sort_.column = SortColumnFromSettings(settings.contentSortColumn);
    sort_.ascending = !settings.contentSortReverse;
}
void FileListView::ConfigureColumns() {
    while (ListView_DeleteColumn(hwnd, 0)) {}
    const auto settings = wit::platform::LoadAppSettings();
    if (ShowsBrowserItems()) {
        InsertColumns(hwnd, kBrowserRootColumns, settings);
        UpdateSortIndicators();
        return;
    }
    InsertColumns(hwnd, kBrowserContentColumns, settings);
    UpdateSortIndicators();
}

bool FileListView::PersistColumnWidths() const {
    if (!hwnd) return false;

    auto settings = wit::platform::LoadAppSettings();
    const auto persist = [&](const auto& columns) {
        for (std::size_t index = 0; index < columns.size(); ++index) {
            const int width = ListView_GetColumnWidth(hwnd, static_cast<int>(index));
            if (IsValidColumnWidth(width)) settings.fileListColumnWidths[columns[index].key] = width;
        }
    };
    if (ShowsBrowserItems()) {
        persist(kBrowserRootColumns);
    } else {
        persist(kBrowserContentColumns);
    }
    return wit::platform::SaveAppSettings(settings);
}

void FileListView::SetLocation(
    const wit::core::BrowserLocation& newLocation, wit::storage::IBrowserRepository* repository) {
    const HWND header = hwnd ? ListView_GetHeader(hwnd) : nullptr;
    if (header && Header_GetItemCount(header) > 0) {
        (void)PersistColumnWidths();
    }
    CancelLocationLoad();
    CancelPageLoad();
    location = newLocation;
    browser = repository;
    total = 0;
    browserPageStart = -1;
    browserPage.clear();
    pendingSelectedEntries_.clear();
    pendingFocusedId_ = 0;
    pendingFocusedIsDirectory_ = false;
    restoreSelectionAfterLoad_ = false;
    loadPageStart_ = 0;
    pendingPageStart_ = -1;
    ClearCache();
    if (hwnd) ListView_SetItemCountEx(hwnd, 0, LVSICF_NOINVALIDATEALL);
    ConfigureColumns();
    BeginLocationLoad();
}void FileListView::ResetCachedItems() {
    CancelLocationLoad();
    CancelPageLoad();
    browserPageStart = -1;
    browserPage.clear();
    ClearCache();
    pendingPageStart_ = -1;
    if (!hwnd) return;
    loadPageStart_ = ((std::max)(0, ListView_GetTopIndex(hwnd)) / PageSize) * PageSize;
    ListView_SetItemCountEx(hwnd, 0, LVSICF_NOINVALIDATEALL);
    total = 0;
    InvalidateRect(hwnd, nullptr, TRUE);
    BeginLocationLoad();
}bool FileListView::ApplyBrowserRootSort() {
    if (!hwnd || !ShowsBrowserItems()) return false;
    CancelLocationLoad();
    CancelPageLoad();
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    loadPageStart_ = ((std::max)(0, ListView_GetTopIndex(hwnd)) / PageSize) * PageSize;
    pendingPageStart_ = -1;
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    BeginLocationLoad();
    return true;
}std::vector<wit::core::FileEntry> FileListView::SelectedEntriesInRange(int firstRow, int lastRow) {
    std::vector<wit::core::FileEntry> selected;
    if (!hwnd || ShowsBrowserItems()) return selected;
    firstRow = std::clamp(firstRow, 0, (std::max)(0, total - 1));
    lastRow = std::clamp(lastRow, firstRow, (std::max)(0, total - 1));
    for (int row = ListView_GetNextItem(hwnd, firstRow - 1, LVNI_SELECTED); row >= 0 && row <= lastRow;
        row = ListView_GetNextItem(hwnd, row, LVNI_SELECTED)) {
        if (const auto* entry = CachedEntryAt(row)) selected.push_back(*entry);
    }
    return selected;
}
bool FileListView::ApplyContentSort(
    std::vector<wit::core::FileEntry> selectedEntries, std::int64_t focusedId, bool focusedIsDirectory) {
    if (!hwnd || ShowsBrowserItems()) return false;
    CancelLocationLoad();
    CancelPageLoad();
    pendingSelectedEntries_ = std::move(selectedEntries);
    pendingFocusedId_ = focusedId;
    pendingFocusedIsDirectory_ = focusedIsDirectory;
    restoreSelectionAfterLoad_ = !pendingSelectedEntries_.empty() || pendingFocusedId_ != 0;
    loadPageStart_ = ((std::max)(0, ListView_GetTopIndex(hwnd)) / PageSize) * PageSize;
    pendingPageStart_ = -1;
    BeginLocationLoad();
    return true;
}bool FileListView::ToggleSortForColumn(int column) {
    if (!hwnd) return false;
    if (ShowsBrowserItems()) {
        if (!IsBrowserRootColumn(column)) return false;
        if (rootSort_.column == column) {
            rootSort_.ascending = !rootSort_.ascending;
        } else {
            rootSort_.column = column;
            rootSort_.ascending = true;
        }
        UpdateSortIndicators();
        return ApplyBrowserRootSort();
    }
    const auto sortColumn = SortColumnFromContentColumn(column);
    if (!sortColumn) return false;
    auto nextSort = sort_;
    if (nextSort.column == *sortColumn) {
        nextSort.ascending = !nextSort.ascending;
    } else {
        nextSort.column = *sortColumn;
        nextSort.ascending = true;
    }
    return SetSort(nextSort);
}

bool FileListView::SetSort(wit::core::FileSort sort) {
    if (!hwnd || ShowsBrowserItems()) {
        sort_ = sort;
        UpdateSortIndicators();
        return false;
    }
    const int focusedRow = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
    const auto* focusedEntry = focusedRow >= 0 ? CachedEntryAt(focusedRow) : nullptr;
    const std::int64_t focusedId = focusedEntry ? focusedEntry->id : 0;
    const bool focusedIsDirectory = focusedEntry && focusedEntry->isDirectory;
    const int topRow = (std::max)(0, ListView_GetTopIndex(hwnd));
    const int visibleRows = (std::max)(ListView_GetCountPerPage(hwnd), 1);
    auto selected = SelectedEntriesInRange((std::max)(0, topRow - PageSize),
        (std::min)(total - 1, topRow + visibleRows + PageSize));

    sort_ = sort;
    UpdateSortIndicators();
    return ApplyContentSort(std::move(selected), focusedId, focusedIsDirectory);
}
bool FileListView::SetRootSort(wit::core::BrowserRootSort sort) {
    rootSort_ = sort;
    UpdateSortIndicators();
    return ApplyBrowserRootSort();
}

void FileListView::UpdateSortIndicators() {
    if (!hwnd) return;
    if (ShowsBrowserItems()) {
        UpdateListViewSortIndicators(hwnd, rootSort_.column, rootSort_.ascending);
        return;
    }
    UpdateListViewSortIndicators(hwnd, ContentColumnFromSortColumn(sort_.column), sort_.ascending);
}

void FileListView::BeginLocationLoad() {
    if (!browser || !hwnd) {
        total = 0;
        if (hwnd) ListView_SetItemCountEx(hwnd, 0, LVSICF_NOINVALIDATEALL);
        return;
    }

    const auto requestId = ++loadRequestId_;
    const auto loadLocation = location;
    const auto fileSort = sort_;
    const auto rootSort = rootSort_;
    const int pageStart = (std::max)(0, loadPageStart_);
    const bool browserItems = ShowsBrowserItems();
    auto* repository = browser;
    auto repositoryMutex = repositoryMutex_;
    const HWND window = hwnd;
    auto mailbox = std::make_shared<AsyncLoadMailbox>();
    loadMailbox_ = mailbox;
    const std::weak_ptr<AsyncLoadMailbox> mailboxReference = mailbox;

    loadWorker_ = std::jthread([requestId, loadLocation, fileSort, rootSort, pageStart, browserItems, repository,
        repositoryMutex, window, mailboxReference](std::stop_token stopToken) {
        AsyncLoadResult result;
        result.requestId = requestId;
        result.pageStart = pageStart;
        result.browserItems = browserItems;
        std::scoped_lock repositoryLock(*repositoryMutex);
        result.total = browserItems ? repository->GetBrowserRootItemCount(loadLocation)
            : repository->GetBrowserItemCount(loadLocation);
        if (stopToken.stop_requested()) return;
        if (result.total > 0) {
            if (browserItems) {
                result.firstBrowserPage = repository->GetBrowserRootItemsPage(loadLocation, pageStart, PageSize, rootSort);
            } else {
                result.firstFilePage = repository->GetBrowserItemsPage(loadLocation, pageStart, PageSize, fileSort);
            }
        }
        if (stopToken.stop_requested()) return;
        PublishLoadResult(mailboxReference, window, std::move(result));
    });
}
void FileListView::CancelLocationLoad() {
    ++loadRequestId_;
    loadMailbox_.reset();
    if (loadWorker_.joinable()) {
        loadWorker_.request_stop();
        RetireWorker(loadWorker_);
    }
}

void FileListView::CancelPageLoad() {
    ++pageRequestId_;
    pageMailbox_.reset();
    if (pageWorker_.joinable()) {
        pageWorker_.request_stop();
        RetireWorker(pageWorker_);
    }
}

void FileListView::RetireWorker(std::jthread& worker) {
    if (!worker.joinable()) return;
    {
        std::scoped_lock lock(reaperMutex_);
        retiredWorkers_.push_back(std::move(worker));
        ++activeRetiredWorkers_;
    }
    reaperCondition_.notify_one();
}

void FileListView::DrainWorkers() {
    std::unique_lock lock(reaperMutex_);
    reaperCondition_.wait(lock, [this]() { return activeRetiredWorkers_ == 0; });
}

void FileListView::ReapWorkers() {
    for (;;) {
        std::jthread retired;
        {
            std::unique_lock lock(reaperMutex_);
            reaperCondition_.wait(lock, [this]() { return stopReaper_ || !retiredWorkers_.empty(); });
            if (retiredWorkers_.empty()) {
                if (stopReaper_) return;
                continue;
            }
            retired = std::move(retiredWorkers_.back());
            retiredWorkers_.pop_back();
        }
        if (retired.joinable()) retired.join();
        {
            std::scoped_lock lock(reaperMutex_);
            if (activeRetiredWorkers_ > 0) --activeRetiredWorkers_;
        }
        reaperCondition_.notify_all();
    }
}

void FileListView::PublishLoadResult(const std::weak_ptr<AsyncLoadMailbox>& mailbox, HWND window,
    AsyncLoadResult result) {
    const auto sharedMailbox = mailbox.lock();
    if (!sharedMailbox) return;
    {
        std::scoped_lock lock(sharedMailbox->mutex);
        sharedMailbox->pendingResult = std::move(result);
    }
    if (window) ::PostMessageW(window, LoadCompleteMessage, 0, 0);
}

void FileListView::PublishPageResult(const std::weak_ptr<AsyncPageMailbox>& mailbox, HWND window,
    AsyncPageResult result) {
    const auto sharedMailbox = mailbox.lock();
    if (!sharedMailbox) return;
    {
        std::scoped_lock lock(sharedMailbox->mutex);
        sharedMailbox->pendingResult = std::move(result);
    }
    if (window) ::PostMessageW(window, PageReadyMessage, 0, 0);
}

LRESULT FileListView::OnLoadComplete() {
    std::optional<AsyncLoadResult> result;
    const auto mailbox = loadMailbox_;
    if (mailbox) {
        std::scoped_lock lock(mailbox->mutex);
        if (mailbox->pendingResult && mailbox->pendingResult->requestId == loadRequestId_) {
            result = std::move(mailbox->pendingResult);
        }
        mailbox->pendingResult.reset();
    }
    if (!result || !hwnd) return 0;
    RetireWorker(loadWorker_);
    if (mailbox == loadMailbox_) loadMailbox_.reset();

    ClearCache();
    browserPageStart = -1;
    browserPage.clear();
    total = result->total;
    if (result->browserItems) {
        browserPageStart = result->firstBrowserPage.empty() ? -1 : result->pageStart;
        browserPage = std::move(result->firstBrowserPage);
    } else if (!result->firstFilePage.empty()) {
        CachedFilePage page;
        page.start = result->pageStart;
        page.items = std::move(result->firstFilePage);
        page.lastUsed = ++cacheClock_;
        cachedFilePages_.push_back(std::move(page));
    }
    ResetItemCache();
    RestorePendingSelection();
    ScheduleVisiblePageLoad();
    return 0;
}

LRESULT FileListView::OnPageReady() {
    std::optional<AsyncPageResult> result;
    const auto mailbox = pageMailbox_;
    if (mailbox) {
        std::scoped_lock lock(mailbox->mutex);
        if (mailbox->pendingResult && mailbox->pendingResult->requestId == pageRequestId_) {
            result = std::move(mailbox->pendingResult);
        }
        mailbox->pendingResult.reset();
    }
    if (!result || !hwnd) return 0;
    RetireWorker(pageWorker_);
    if (mailbox == pageMailbox_) pageMailbox_.reset();

    int first{};
    int last{-1};
    if (result->browserItems) {
        browserPageStart = result->browserPageStart;
        browserPage = std::move(result->browserPage);
        first = browserPageStart;
        last = first + static_cast<int>(browserPage.size()) - 1;
    } else {
        auto existing = std::ranges::find_if(cachedFilePages_,
            [start = result->filePage.start](const CachedFilePage& page) { return page.start == start; });
        result->filePage.lastUsed = ++cacheClock_;
        first = result->filePage.start;
        last = first + static_cast<int>(result->filePage.items.size()) - 1;
        if (existing != cachedFilePages_.end()) {
            *existing = std::move(result->filePage);
        } else {
            cachedFilePages_.push_back(std::move(result->filePage));
        }
        while (cachedFilePages_.size() > MaxCachedPages) {
            const auto oldest = std::ranges::min_element(cachedFilePages_,
                [](const CachedFilePage& left, const CachedFilePage& right) { return left.lastUsed < right.lastUsed; });
            if (oldest == cachedFilePages_.end()) break;
            cachedFilePages_.erase(oldest);
        }
    }
    if (last >= first) ListView_RedrawItems(hwnd, first, last);
    if (pendingPageStart_ >= 0) {
        const int pending = std::exchange(pendingPageStart_, -1);
        SchedulePageLoad(pending);
    } else {
        ScheduleVisiblePageLoad();
    }
    return 0;
}

void FileListView::ResetItemCache() {
    if (!hwnd) return;
    ListView_SetItemCountEx(hwnd, total, LVSICF_NOSCROLL);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void FileListView::RestorePendingSelection() {
    if (!hwnd || ShowsBrowserItems() || !restoreSelectionAfterLoad_) return;
    auto selectedEntries = std::move(pendingSelectedEntries_);
    const auto focusedId = pendingFocusedId_;
    const bool focusedIsDirectory = pendingFocusedIsDirectory_;
    pendingSelectedEntries_.clear();
    pendingFocusedId_ = 0;
    pendingFocusedIsDirectory_ = false;
    restoreSelectionAfterLoad_ = false;

    const int topRow = (std::max)(0, ListView_GetTopIndex(hwnd));
    const int visibleRows = (std::max)(ListView_GetCountPerPage(hwnd), 1);
    const int firstRestoreRow = (std::max)(0, topRow - PageSize);
    const int lastRestoreRow = (std::min)(total - 1, topRow + visibleRows + PageSize);
    bool focusedRestored = focusedId == 0;
    for (int row = firstRestoreRow; row <= lastRestoreRow && (!selectedEntries.empty() || !focusedRestored); ++row) {
        const auto* entry = EntryAt(row);
        if (!entry) continue;
        const auto selected = std::ranges::find_if(selectedEntries, [entry](const auto& selectedEntry) {
            return selectedEntry.id == entry->id && selectedEntry.isDirectory == entry->isDirectory;
        });
        if (selected != selectedEntries.end()) {
            ListView_SetItemState(hwnd, row, LVIS_SELECTED, LVIS_SELECTED);
            selectedEntries.erase(selected);
        }
        if (!focusedRestored && entry->id == focusedId && entry->isDirectory == focusedIsDirectory) {
            ListView_SetItemState(hwnd, row, LVIS_FOCUSED, LVIS_FOCUSED);
            ListView_EnsureVisible(hwnd, row, FALSE);
            focusedRestored = true;
        }
    }
}

LRESULT CALLBACK FileListView::ListSubclassProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
    UINT_PTR subclassId, DWORD_PTR referenceData) {
    auto* list = reinterpret_cast<FileListView*>(referenceData);
    if (subclassId == SubclassId && list) {
        if (message == LoadCompleteMessage) return list->OnLoadComplete();
        if (message == PageReadyMessage) return list->OnPageReady();
        if (message == WM_NCDESTROY) {
            RemoveWindowSubclass(window, ListSubclassProc, subclassId);
            list->hwnd = nullptr;
        }
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

void FileListView::ScheduleVisiblePageLoad() {
    if (!hwnd || total <= 0) return;
    const int topRow = (std::max)(0, ListView_GetTopIndex(hwnd));
    const int visibleRows = (std::max)(ListView_GetCountPerPage(hwnd), 1);
    PreloadRange(topRow, (std::min)(total - 1, topRow + visibleRows));
}

void FileListView::SchedulePageLoad(int pageStartValue) {
    if (!browser || pageStartValue < 0 || pageStartValue >= total) return;
    const int normalizedStart = (pageStartValue / PageSize) * PageSize;
    if (ShowsBrowserItems()) {
        if (browserPageStart == normalizedStart) return;
    } else {
        const auto cached = std::ranges::find_if(cachedFilePages_,
            [normalizedStart](const CachedFilePage& page) { return page.start == normalizedStart; });
        if (cached != cachedFilePages_.end()) return;
    }
    if (pageWorker_.joinable()) {
        pendingPageStart_ = normalizedStart;
        return;
    }

    const auto requestId = ++pageRequestId_;
    const auto pageLocation = location;
    const auto fileSort = sort_;
    const auto rootSort = rootSort_;
    const bool browserItems = ShowsBrowserItems();
    auto* repository = browser;
    auto repositoryMutex = repositoryMutex_;
    const HWND window = hwnd;
    auto mailbox = std::make_shared<AsyncPageMailbox>();
    pageMailbox_ = mailbox;
    const std::weak_ptr<AsyncPageMailbox> mailboxReference = mailbox;

    pageWorker_ = std::jthread([requestId, normalizedStart, pageLocation, fileSort, rootSort, browserItems,
        repository, repositoryMutex, window, mailboxReference](std::stop_token stopToken) {
        AsyncPageResult result;
        result.requestId = requestId;
        result.browserItems = browserItems;
        std::scoped_lock repositoryLock(*repositoryMutex);
        if (browserItems) {
            result.browserPageStart = normalizedStart;
            result.browserPage = repository->GetBrowserRootItemsPage(pageLocation, normalizedStart, PageSize, rootSort);
        } else {
            result.filePage.start = normalizedStart;
            result.filePage.items = repository->GetBrowserItemsPage(pageLocation, normalizedStart, PageSize, fileSort);
        }
        if (stopToken.stop_requested()) return;
        PublishPageResult(mailboxReference, window, std::move(result));
    });
}

void FileListView::PreloadRange(int firstRow, int lastRow) {
    if (total <= 0) return;
    firstRow = std::clamp(firstRow, 0, total - 1);
    lastRow = std::clamp(lastRow, firstRow, total - 1);

    const int firstPage = firstRow / PageSize;
    const int lastPage = lastRow / PageSize;
    for (int page = firstPage; page <= lastPage; ++page) {
        const int start = page * PageSize;
        if (ShowsBrowserItems()) {
            if (browserPageStart != start) {
                SchedulePageLoad(start);
                return;
            }
        } else {
            const auto cached = std::ranges::find_if(cachedFilePages_,
                [start](const CachedFilePage& cachedPage) { return cachedPage.start == start; });
            if (cached == cachedFilePages_.end()) {
                SchedulePageLoad(start);
                return;
            }
        }
    }
}
void FileListView::ClearCache() {
    cacheClock_ = 0;
    cachedFilePages_.clear();
}

void FileListView::CacheFilePage(int pageStartValue) {
    if (!browser || ShowsBrowserItems() || pageStartValue < 0 || pageStartValue >= total) return;

    const int normalizedStart = (pageStartValue / PageSize) * PageSize;
    const auto found = std::ranges::find_if(cachedFilePages_,
        [normalizedStart](const CachedFilePage& cachedPage) { return cachedPage.start == normalizedStart; });
    if (found != cachedFilePages_.end()) {
        found->lastUsed = ++cacheClock_;
        return;
    }

    CachedFilePage cachedPage;
    cachedPage.start = normalizedStart;
    {
        std::scoped_lock repositoryLock(*repositoryMutex_);
        cachedPage.items = browser->GetBrowserItemsPage(location, normalizedStart, PageSize, sort_);
    }
    cachedPage.lastUsed = ++cacheClock_;
    cachedFilePages_.push_back(std::move(cachedPage));

    while (cachedFilePages_.size() > MaxCachedPages) {
        const auto oldest = std::ranges::min_element(cachedFilePages_,
            [](const CachedFilePage& left, const CachedFilePage& right) { return left.lastUsed < right.lastUsed; });
        if (oldest == cachedFilePages_.end()) break;
        cachedFilePages_.erase(oldest);
    }
}

const wit::core::FileEntry* FileListView::CachedEntryAt(int row) {
    if (!browser || ShowsBrowserItems() || row < 0 || row >= total) return nullptr;
    const int pageStart = (row / PageSize) * PageSize;
    const auto found = std::ranges::find_if(cachedFilePages_,
        [pageStart](const CachedFilePage& cachedPage) { return cachedPage.start == pageStart; });
    if (found == cachedFilePages_.end()) return nullptr;
    found->lastUsed = ++cacheClock_;
    const int index = row - found->start;
    return index >= 0 && index < static_cast<int>(found->items.size()) ? &found->items[index] : nullptr;
}

const wit::core::FileEntry* FileListView::EntryAt(int row) {
    if (!browser || ShowsBrowserItems() || row < 0 || row >= total) return nullptr;
    const int pageStart = (row / PageSize) * PageSize;
    CacheFilePage(pageStart);
    return CachedEntryAt(row);
}
const wit::core::Disk* FileListView::DiskAt(int row) {
    const auto* item = BrowserItemAt(row);
    return item && item->type == wit::core::BrowserItemType::Disk ? &item->disk : nullptr;
}

const wit::core::BrowserItem* FileListView::CachedBrowserItemAt(int row) {
    if (!browser || !ShowsBrowserItems() || row < 0 || row >= total) return nullptr;
    const int pageStart = (row / PageSize) * PageSize;
    if (browserPageStart != pageStart) return nullptr;
    const int index = row - pageStart;
    return index >= 0 && index < static_cast<int>(browserPage.size()) ? &browserPage[index] : nullptr;
}

const wit::core::BrowserItem* FileListView::BrowserItemAt(int row) {
    if (!browser || !ShowsBrowserItems() || row < 0 || row >= total) return nullptr;
    const int pageStart = (row / PageSize) * PageSize;
    if (browserPageStart != pageStart) {
        {
            std::scoped_lock repositoryLock(*repositoryMutex_);
            browserPage = browser->GetBrowserRootItemsPage(location, pageStart, PageSize, rootSort_);
        }
        browserPageStart = pageStart;
    }
    return CachedBrowserItemAt(row);
}
bool FileListView::SelectEntry(std::int64_t id, bool isDirectory) {
    if (!hwnd || ShowsBrowserItems()) return false;
    for (int row = 0; row < total; ++row) {
        const auto* entry = EntryAt(row);
        if (!entry || entry->id != id || entry->isDirectory != isDirectory) continue;
        ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_SetItemState(hwnd, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(hwnd, row, FALSE);
        return true;
    }
    return false;
}

int FileListView::ImageFor(int row) {
    if (ShowsBrowserItems()) {
        const auto* item = CachedBrowserItemAt(row);
        if (!item) {
            SchedulePageLoad(row);
            return I_IMAGENONE;
        }
        return item->type == wit::core::BrowserItemType::DiskGroup ? BrowserFolderImage : BrowserDriveImage;
    }
    const auto* entry = CachedEntryAt(row);
    if (!entry) SchedulePageLoad(row);
    return entry ? ImageForBrowserEntry(*entry) : I_IMAGENONE;
}
void FileListView::TextFor(int row, int column, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    buffer[0] = L'\0';
    if (ShowsDisks()) {
        const auto* item = CachedBrowserItemAt(row);
        if (!item) {
            SchedulePageLoad(row);
            return;
        }
        if (item->type == wit::core::BrowserItemType::DiskGroup) {
            switch (column) {
            case 0: CopyText(item->group.name, buffer, bufferSize); return;
            case 1: CopyText(L"Disk Group", buffer, bufferSize); return;
            case 2: wit::core::FormatSizeToBuffer(item->group.totalCapacity, buffer, bufferSize); return;
            case 3: wit::core::FormatSizeToBuffer(item->group.freeSpace, buffer, bufferSize); return;
            case 4: wit::platform::FormatUnixTimestampToBuffer(item->group.updatedAt, buffer, bufferSize); return;
            case 5: swprintf_s(buffer, bufferSize, L"%lld", static_cast<long long>(item->group.totalDisks)); return;
            default: return;
            }
        }
        const auto* disk = &item->disk;
        switch (column) {
        case 0: CopyText(disk->diskName, buffer, bufferSize); return;
        case 1: CopyText(DiskTypeLabel(disk->diskType), buffer, bufferSize); return;
        case 2: wit::core::FormatSizeToBuffer(disk->totalCapacity, buffer, bufferSize); return;
        case 3: wit::core::FormatSizeToBuffer(disk->freeSpace, buffer, bufferSize); return;
        case 4: wit::platform::FormatUnixTimestampToBuffer(disk->updatedAt, buffer, bufferSize); return;
        case 5: swprintf_s(buffer, bufferSize, L"%lld", static_cast<long long>(disk->diskNumber)); return;
        case 6: CopyText(disk->description, buffer, bufferSize); return;
        case 7: CopyText(disk->category, buffer, bufferSize); return;
        case 8: CopyText(disk->location, buffer, bufferSize); return;
        default: return;
        }
    }
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
#include "wit_gui/FileListPane.h"
#include <CommCtrl.h>

namespace wit::ui {
void CatalogListView::Reload() {
    ListView_DeleteAllItems(hwnd);
    for (std::size_t index = 0; index < catalogs.size(); ++index) {
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(index);
        item.pszText = const_cast<LPWSTR>(catalogs[index].name.c_str());
        item.lParam = static_cast<LPARAM>(catalogs[index].id);
        ListView_InsertItem(hwnd, &item);
    }
}
}


