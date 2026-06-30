#pragma once
#include <Windows.h>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "wit_types/BrowserLocation.h"
#include <wit_types/Disk.h>
#include <wit_types/FileEntry.h>
#include <wit_types/FileSort.h>
#include "wit_database/IBrowserRepository.h"

namespace wit::ui {
std::wstring CompactFileSize(std::uint64_t bytes);
std::wstring_view FileEntryTypeText(const wit::core::FileEntry& entry);
std::wstring FileEntryStatusText(const wit::core::FileEntry& entry);
class FileListView {
public:
    FileListView();
    ~FileListView();

    HWND hwnd{};
    wit::core::BrowserLocation location;
    wit::storage::IBrowserRepository* browser{};
    int total{};
    int browserPageStart{-1};
    std::vector<wit::core::BrowserItem> browserPage;

    void Attach(HWND handle);
    void SetLocation(const wit::core::BrowserLocation& newLocation, wit::storage::IBrowserRepository* repository);
    void ResetCachedItems();
    void PreloadRange(int firstRow, int lastRow);
    [[nodiscard]] bool PersistColumnWidths() const;
    bool ShowsBrowserItems() const { return browser && (location.isRoot || location.isDiskGroup); }
    bool ShowsDisks() const { return ShowsBrowserItems(); }
    const wit::core::FileEntry* CachedEntryAt(int row);
    const wit::core::FileEntry* EntryAt(int row);
    const wit::core::BrowserItem* CachedBrowserItemAt(int row);
    const wit::core::BrowserItem* BrowserItemAt(int row);
    const wit::core::Disk* DiskAt(int row);
    bool SelectEntry(std::int64_t id, bool isDirectory);
    bool ToggleSortForColumn(int column);
    bool SetSort(wit::core::FileSort sort);
    bool SetRootSort(wit::core::BrowserRootSort sort);
    [[nodiscard]] wit::core::FileSort Sort() const { return sort_; }
    [[nodiscard]] wit::core::BrowserRootSort RootSort() const { return rootSort_; }
    void UpdateSortIndicators();
    int ImageFor(int row);
    void TextFor(int row, int column, wchar_t* buffer, std::size_t bufferSize);

private:
    struct CachedFilePage {
        int start{};
        std::vector<wit::core::FileEntry> items;
        unsigned long long lastUsed{};
    };

    struct AsyncLoadResult {
        std::uint64_t requestId{};
        int total{};
        int pageStart{};
        bool browserItems{};
        std::vector<wit::core::FileEntry> firstFilePage;
        std::vector<wit::core::BrowserItem> firstBrowserPage;
        std::wstring errorMessage;
    };

    struct AsyncPageResult {
        std::uint64_t requestId{};
        bool browserItems{};
        CachedFilePage filePage;
        int browserPageStart{-1};
        std::vector<wit::core::BrowserItem> browserPage;
        std::wstring errorMessage;
    };

    struct AsyncLoadMailbox {
        std::mutex mutex;
        std::optional<AsyncLoadResult> pendingResult;
    };

    struct AsyncPageMailbox {
        std::mutex mutex;
        std::optional<AsyncPageResult> pendingResult;
    };

    static constexpr int PageSize = 512;
    static constexpr std::size_t MaxCachedPages = 16;
    static constexpr UINT LoadCompleteMessage = WM_APP + 47;
    static constexpr UINT PageReadyMessage = WM_APP + 48;
    static constexpr UINT_PTR SubclassId = 7;

    unsigned long long cacheClock_{};
    std::vector<CachedFilePage> cachedFilePages_;
    wit::core::FileSort sort_{};
    wit::core::BrowserRootSort rootSort_{};
    std::jthread loadWorker_;
    std::shared_ptr<AsyncLoadMailbox> loadMailbox_;
    std::jthread pageWorker_;
    std::shared_ptr<AsyncPageMailbox> pageMailbox_;
    std::uint64_t loadRequestId_{};
    std::uint64_t pageRequestId_{};
    std::mutex reaperMutex_;
    std::condition_variable reaperCondition_;
    std::vector<std::jthread> retiredWorkers_;
    std::size_t activeRetiredWorkers_{};
    bool stopReaper_{};
    std::jthread reaper_;
    std::shared_ptr<std::mutex> repositoryMutex_{std::make_shared<std::mutex>()};
    std::vector<wit::core::FileEntry> pendingSelectedEntries_;
    std::int64_t pendingFocusedId_{};
    bool pendingFocusedIsDirectory_{};
    bool restoreSelectionAfterLoad_{};
    int loadPageStart_{};
    int pendingPageStart_{-1};
    std::wstring browserErrorMessage_;

    void ConfigureColumns();
    void ClearCache();
    void CacheFilePage(int pageStart);
    void SchedulePageLoad(int pageStart);
    void BeginLocationLoad();
    void ScheduleVisiblePageLoad();
    void CancelLocationLoad();
    void CancelPageLoad();
    void RetireWorker(std::jthread& worker);
    void DrainWorkers();
    void ReapWorkers();
    static void PublishLoadResult(const std::weak_ptr<AsyncLoadMailbox>& mailbox, HWND window,
        AsyncLoadResult result);
    static void PublishPageResult(const std::weak_ptr<AsyncPageMailbox>& mailbox, HWND window,
        AsyncPageResult result);
    LRESULT OnLoadComplete();
    LRESULT OnPageReady();
    void ResetItemCache();
    void RestorePendingSelection();
    static LRESULT CALLBACK ListSubclassProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR subclassId, DWORD_PTR referenceData);
    bool ApplyBrowserRootSort();
    bool ApplyContentSort(std::vector<wit::core::FileEntry> selectedEntries, std::int64_t focusedId, bool focusedIsDirectory);
    std::vector<wit::core::FileEntry> SelectedEntriesInRange(int firstRow, int lastRow);
};
}
#include <Windows.h>
#include <vector>
#include "wit_catalog/Catalog.h"

namespace wit::ui {
class CatalogListView {
public:
    HWND hwnd{};
    std::vector<wit::core::Catalog> catalogs;

    void Attach(HWND handle) { hwnd = handle; }
    void Reload();
};
}


