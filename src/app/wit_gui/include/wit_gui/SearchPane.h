#pragma once
#include "wit_win32/BaseWindow.h"
#include "resource.h"
#include <wit_types/FileEntry.h>
#include "wit_search/AdvancedSearchParser.h"
#include <wit_types/FileSort.h>
#include "wit_search/ISearchRepository.h"
#include <CommCtrl.h>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <string>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace wit::ui {
class SearchDialog : public ATL::CDialogImpl<SearchDialog>, public WTL::CDialogResize<SearchDialog> {
public:
    enum { IDD = IDD_SEARCH_ITEMS };
    SearchDialog();
    ~SearchDialog();
    static constexpr UINT SearchCompleteMessage = WM_APP + 44;
    static constexpr UINT PersistColumnWidthsMessage = WM_APP + 45;
    static constexpr UINT PageReadyMessage = WM_APP + 46;

    using LocateResultHandler = std::function<bool(const wit::core::FileEntry&)>;

    bool Show(HWND owner, wit::search::ISearchRepository* search, LocateResultHandler onLocate,
        std::function<void()> onClose = {});
    bool Show(HWND owner, wit::search::ISearchRepository* search, std::function<void()> onClose);
    void Close();
    void RefreshDisplay();
    BOOL PreTranslateMessage(MSG* message);

    BEGIN_MSG_MAP(SearchDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
        MESSAGE_HANDLER(WM_CLOSE, OnWindowClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(SearchCompleteMessage, OnSearchComplete)
        MESSAGE_HANDLER(PersistColumnWidthsMessage, OnPersistColumnWidths)
        MESSAGE_HANDLER(PageReadyMessage, OnPageReady)
        COMMAND_ID_HANDLER(IDC_SEARCH_EXECUTE, OnExecuteSearch)
        COMMAND_ID_HANDLER(IDC_ADVANCED_SEARCH_EXECUTE, OnExecuteAdvancedSearch)
        COMMAND_ID_HANDLER(IDC_ADVANCED_SEARCH_CLEAR, OnClearAdvancedSearch)
        COMMAND_ID_HANDLER(IDCANCEL, OnCloseCommand)
        COMMAND_ID_HANDLER(ID_SEARCH_RESULTS_LOCATE_IN_CATALOG, OnLocateInCatalog)
        NOTIFY_HANDLER(IDC_SEARCH_TABS, TCN_SELCHANGE, OnTabChanged)
        COMMAND_ID_HANDLER(ID_SEARCH_RESULTS_OPEN_EXPLORER_PLACEHOLDER, OnOpenInExplorer)
        NOTIFY_HANDLER(IDC_SEARCH_RESULTS, LVN_GETDISPINFOW, OnGetDisplayInfo)
        NOTIFY_HANDLER(IDC_SEARCH_RESULTS, LVN_ODCACHEHINT, OnCacheHint)
        NOTIFY_HANDLER(IDC_SEARCH_RESULTS, LVN_COLUMNCLICK, OnColumnClick)
        NOTIFY_HANDLER(IDC_SEARCH_RESULTS, LVN_ITEMCHANGED, OnResultItemChanged)
        NOTIFY_CODE_HANDLER(HDN_ENDTRACKW, OnHeaderWidthChanged)
        NOTIFY_CODE_HANDLER(HDN_DIVIDERDBLCLICKW, OnHeaderWidthChanged)
        CHAIN_MSG_MAP(WTL::CDialogResize<SearchDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(SearchDialog)
        DLGRESIZE_CONTROL(IDC_SEARCH_NAME, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SEARCH_EXECUTE, DLSZ_MOVE_X)
        DLGRESIZE_CONTROL(IDC_SEARCH_TABS, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_ADVANCED_SEARCH_QUERY, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_ADVANCED_SEARCH_EXECUTE, DLSZ_MOVE_X)
        DLGRESIZE_CONTROL(IDC_ADVANCED_SEARCH_CLEAR, DLSZ_MOVE_X)
        DLGRESIZE_CONTROL(IDC_ADVANCED_SEARCH_HELP, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SEARCH_SUMMARY, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SEARCH_RESULTS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDC_SEARCH_STATUS, DLSZ_SIZE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

private:
    struct CachedPage {
        int start{};
        std::vector<wit::core::FileEntry> items;
        unsigned long long lastUsed{};
    };

    enum class ResultMode {
        Quick,
        Advanced
    };

    struct AsyncSearchResult {
        std::uint64_t requestId{};
        int total{};
        std::vector<wit::core::FileEntry> firstPage;
        std::wstring error;
        double elapsedSeconds{};
    };

    struct AsyncPageResult {
        std::uint64_t requestId{};
        CachedPage page;
        std::wstring error;
    };

    struct AsyncPageMailbox {
        std::mutex mutex;
        std::optional<AsyncPageResult> pendingResult;
    };

    struct AsyncSearchMailbox {
        std::mutex mutex;
        std::optional<AsyncSearchResult> pendingResult;
    };

    static constexpr int PageSize = 512;
    static constexpr std::size_t MaxCachedPages = 16;

    HWND results_{};
    HWND status_{};
    HIMAGELIST searchImages_{};
    HWND launchOwner_{};
    wit::search::ISearchRepository* search_{};
    LocateResultHandler onLocate_;
    std::function<void()> onClose_;
    std::wstring nameTerm_;
    bool caseSensitive_{};
    wit::search::AdvancedSearchExpression advancedExpression_;
    ResultMode resultMode_{ResultMode::Quick};
    int total_{};
    wit::core::FileSort sort_{};
    unsigned long long cacheClock_{};
    std::vector<CachedPage> cachedPages_;
    std::jthread searchWorker_;
    std::shared_ptr<AsyncSearchMailbox> searchMailbox_;
    std::jthread pageWorker_;
    std::shared_ptr<AsyncPageMailbox> pageMailbox_;
    std::uint64_t pageRequestId_{};
    std::mutex searchReaperMutex_;
    std::condition_variable searchReaperCondition_;
    std::vector<std::jthread> retiredSearchWorkers_;
    std::size_t activeRetiredSearchWorkers_{};
    bool stopSearchReaper_{};
    std::jthread searchReaper_;
    std::uint64_t searchRequestId_{};
    double elapsedSeconds_{};
    std::vector<std::wstring> quickSearchHistory_;
    int quickSearchHistoryIndex_{-1};
    std::wstring quickSearchHistoryDraft_;

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnContextMenu(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnExecuteSearch(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnExecuteAdvancedSearch(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnClearAdvancedSearch(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnLocateInCatalog(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnOpenInExplorer(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnWindowClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSearchComplete(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnPersistColumnWidths(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnPageReady(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCloseCommand(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnGetDisplayInfo(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnCacheHint(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnTabChanged(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnColumnClick(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnResultItemChanged(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnHeaderWidthChanged(int id, LPNMHDR header, BOOL& handled);
    static LRESULT CALLBACK HeaderSubclassProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR subclassId, DWORD_PTR referenceData);
    static LRESULT CALLBACK SearchNameSubclassProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR subclassId, DWORD_PTR referenceData);
    void Initialize();
    void Search();
    void RememberQuickSearchQuery(const std::wstring& query);
    bool NavigateQuickSearchHistory(int direction);
    void SetQuickSearchTextAtEnd(const std::wstring& text);
    void AdvancedSearch();
    void BeginSearchLoad();
    void CancelSearchLoad();
    void CancelPageLoad();
    void RetireWorker(std::jthread& worker);
    void DrainWorkers();
    void ReapWorkers();
    static void PublishSearchResult(const std::weak_ptr<AsyncSearchMailbox>& mailbox, HWND window,
        AsyncSearchResult result);
    static void PublishPageResult(const std::weak_ptr<AsyncPageMailbox>& mailbox, HWND window,
        AsyncPageResult result);
    void ShowTabPage(int index);
    std::wstring DialogText(int controlId) const;
    void ClearCache();
    void ResetResultItemCache();
    void CachePage(int pageStart);
    void SchedulePageLoad(int pageStart);
    void PreloadRange(int firstRow, int lastRow);
    const wit::core::FileEntry* CachedEntryAt(int row);
    const wit::core::FileEntry* EntryAt(int row);
    const wit::core::FileEntry* FocusedEntry();
    void ToggleSortForColumn(int column);
    void UpdateSortIndicators();
    void UpdateStatusParts();
    void UpdateStatusText();
    bool PersistColumnWidths() const;
    std::vector<wit::core::FileEntry> SelectedEntriesInRange(int firstRow, int lastRow);
    void RestoreSelection(std::vector<wit::core::FileEntry> selectedEntries,
        std::int64_t focusedId, bool focusedIsDirectory);
    bool PrepareContextMenuSelection(LPARAM lparam, POINT& screenPoint);
    void ShowResultsContextMenu(POINT screenPoint);
    void TextFor(int row, int column, wchar_t* buffer, std::size_t bufferSize);
};
}
