#include <gtest/gtest.h>
#include <wit_database/Database.h>
#include <wit_infra/AppSettings.h>
#include <wit_gui/BrowserItemIcons.h>
#include <wit_gui/FileListPane.h>
#include <wit_gui/SearchPane.h>
#include <CommCtrl.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

WTL::CAppModule _Module;

namespace {
class AppSettingsGuard {
public:
    AppSettingsGuard() : original_(wit::platform::LoadAppSettings()) {}
    ~AppSettingsGuard() { (void)wit::platform::SaveAppSettings(original_); }

private:
    wit::platform::AppSettings original_;
};
class AtlModuleGuard {
public:
    AtlModuleGuard() {
        initialized_ = SUCCEEDED(_Module.Init(nullptr, GetModuleHandleW(nullptr)));
    }


    bool initialized() const { return initialized_; }

private:
    bool initialized_{};
};

void PumpMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

class BlockingSearchRepository final : public wit::search::ISearchRepository {
public:
    wit::search::PreparedSearchResult PrepareByName(
        const std::wstring& nameTerm, int limit, wit::core::FileSort sort, bool caseSensitive) override {
        wit::search::PreparedSearchResult result;
        result.total = CountByName(nameTerm, caseSensitive);
        if (result.total > 0) result.entries = PageByName(nameTerm, 0, limit, sort, caseSensitive);
        return result;
    }

    wit::search::PreparedSearchResult PrepareAdvanced(
        const wit::search::AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort) override {
        wit::search::PreparedSearchResult result;
        result.total = CountAdvanced(expression);
        if (result.total > 0) result.entries = PageAdvanced(expression, 0, limit, sort);
        return result;
    }

    int CountByName(const std::wstring&, bool) override {
        countStarted = true;
        while (!cancelled.load()) std::this_thread::yield();
        return 0;
    }

    std::vector<wit::core::FileEntry> PageByName(
        const std::wstring&, int, int, wit::core::FileSort, bool) override {
        return {};
    }

    int CountAdvanced(const wit::search::AdvancedSearchExpression&) override {
        return CountByName({}, false);
    }

    std::vector<wit::core::FileEntry> PageAdvanced(
        const wit::search::AdvancedSearchExpression&, int, int, wit::core::FileSort) override {
        return {};
    }

    void CancelPending() override {
        cancelled = true;
        ++cancelCalls;
    }

    std::wstring LastErrorMessage() const override { return {}; }

    std::atomic_bool countStarted{};
    std::atomic_bool cancelled{};
    std::atomic_int cancelCalls{};
};

class ImmediateSearchRepository final : public wit::search::ISearchRepository {
public:
    wit::search::PreparedSearchResult PrepareByName(
        const std::wstring& nameTerm, int limit, wit::core::FileSort sort, bool caseSensitive) override {
        wit::search::PreparedSearchResult result;
        result.total = CountByName(nameTerm, caseSensitive);
        if (result.total > 0) result.entries = PageByName(nameTerm, 0, limit, sort, caseSensitive);
        return result;
    }

    wit::search::PreparedSearchResult PrepareAdvanced(
        const wit::search::AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort) override {
        wit::search::PreparedSearchResult result;
        result.total = CountAdvanced(expression);
        if (result.total > 0) result.entries = PageAdvanced(expression, 0, limit, sort);
        return result;
    }

    int CountByName(const std::wstring&, bool) override { return 1; }

    std::vector<wit::core::FileEntry> PageByName(
        const std::wstring&, int, int, wit::core::FileSort, bool) override {
        wit::core::FileEntry entry;
        entry.name = L"replacement.txt";
        entry.extension = L"txt";
        entry.size = 700ull * 1024ull * 1024ull;
        entry.modifiedAt = 1700000000;
        return {std::move(entry)};
    }

    int CountAdvanced(const wit::search::AdvancedSearchExpression&) override { return 1; }

    std::vector<wit::core::FileEntry> PageAdvanced(
        const wit::search::AdvancedSearchExpression&, int, int, wit::core::FileSort) override {
        return PageByName({}, 0, 1, {}, false);
    }

    void CancelPending() override {}
    std::wstring LastErrorMessage() const override { return {}; }
};

std::wstring StatusPartText(HWND status, int part) {
    const auto length = LOWORD(SendMessageW(status, SB_GETTEXTLENGTHW, part, 0));
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    SendMessageW(status, SB_GETTEXTW, part, reinterpret_cast<LPARAM>(text.data()));
    text.resize(length);
    return text;
}

std::wstring WindowText(HWND window) {
    const int length = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

DWORD SelectionStart(HWND window) {
    DWORD start{};
    DWORD end{};
    SendMessageW(window, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    return start;
}

template <typename Func>
double MeasureMilliseconds(Func&& func) {
    const auto started = std::chrono::steady_clock::now();
    func();
    PumpMessages();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return std::chrono::duration<double, std::milli>(elapsed).count();
}
}

TEST(SearchPaneIcons, OwnsAnIndependentImageList) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    const auto owner = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
        0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(owner, nullptr);
    const auto browserList = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | LVS_REPORT | LVS_SHAREIMAGELISTS, 0, 0, 100, 100,
        owner, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FILES)),
        GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(browserList, nullptr);
    const auto browserImages = ImageList_Create(16, 16, ILC_COLOR32, 1, 0);
    ASSERT_NE(browserImages, nullptr);
    HBITMAP bitmap = CreateBitmap(16, 16, 1, 32, nullptr);
    ASSERT_NE(bitmap, nullptr);
    ASSERT_EQ(ImageList_Add(browserImages, bitmap, nullptr), 0);
    DeleteObject(bitmap);
    ListView_SetImageList(browserList, browserImages, LVSIL_SMALL);

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(owner, &repository, [] {}));
    PumpMessages();
    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto searchList = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    ASSERT_NE(searchList, nullptr);
    const auto searchImages = ListView_GetImageList(searchList, LVSIL_SMALL);
    ASSERT_NE(searchImages, nullptr);
    EXPECT_NE(searchImages, browserImages);
    EXPECT_EQ(ImageList_GetImageCount(searchImages), 105);

    dialog.Close();
    PumpMessages();
    EXPECT_EQ(ImageList_GetImageCount(browserImages), 1)
        << "closing search must not destroy the tree/main-list image list";

    ImageList_Destroy(browserImages);
    DestroyWindow(owner);
}
TEST(SearchPaneLifetime, RebindingCancelsTheOldRepositoryBeforeReplacement) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    BlockingSearchRepository first;
    ImmediateSearchRepository second;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &first, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    ASSERT_TRUE(SetDlgItemTextW(searchWindow, IDC_SEARCH_NAME, L"*"));
    SendMessageW(GetDlgItem(searchWindow, IDC_SEARCH_EXECUTE), BM_CLICK, 0, 0);
    while (!first.countStarted.load()) std::this_thread::yield();

    ASSERT_TRUE(dialog.Show(nullptr, &second, [] {}));
    EXPECT_GT(first.cancelCalls.load(), 0);

    const auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(results) != 1 && std::chrono::steady_clock::now() < deadline) {
        PumpMessages();
        Sleep(1);
    }
    EXPECT_EQ(ListView_GetItemCount(results), 1);

    dialog.Close();
    PumpMessages();
}

TEST(SearchPaneColumns, LoadsAndPersistsIndependentWidths) {
    AppSettingsGuard settingsGuard;
    auto settings = wit::platform::LoadAppSettings();
    settings.fileListColumnWidths[L"BrowserContent.Name"] = 222;
    settings.searchListColumnWidths[L"SearchResults.Name"] = 321;
    ASSERT_TRUE(wit::platform::SaveAppSettings(settings));

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);
    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);
    fileListView.SetLocation({}, nullptr);
    EXPECT_EQ(ListView_GetColumnWidth(fileList, 0), 222);
    DestroyWindow(fileList);

    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();

    auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(ListView_GetColumnWidth(results, 0), 321);

    constexpr std::array<int, 5> widths{333, 144, 155, 366, 177};
    constexpr std::array<const wchar_t*, 5> keys{
        L"SearchResults.Name", L"SearchResults.Type", L"SearchResults.Size",
        L"SearchResults.Path", L"SearchResults.Modified"
    };
    for (std::size_t index = 0; index < widths.size(); ++index) {
        ASSERT_TRUE(ListView_SetColumnWidth(results, static_cast<int>(index), widths[index]));
    }

    // Persist when the user releases the mouse after dragging a real search header divider.
    SendMessageW(ListView_GetHeader(results), WM_LBUTTONUP, 0, 0);
    PumpMessages();

    auto persisted = wit::platform::LoadAppSettings();
    for (std::size_t index = 0; index < keys.size(); ++index) {
        const auto saved = persisted.searchListColumnWidths.find(keys[index]);
        ASSERT_NE(saved, persisted.searchListColumnWidths.end());
        EXPECT_EQ(saved->second, widths[index]);
    }
    EXPECT_EQ(persisted.fileListColumnWidths.at(L"BrowserContent.Name"), 222);

    // Saving a partial or stale settings snapshot must never delete existing column widths.
    persisted.fileListColumnWidths.erase(L"BrowserContent.Name");
    persisted.searchListColumnWidths.erase(L"SearchResults.Path");
    ASSERT_TRUE(wit::platform::SaveAppSettings(persisted));
    persisted = wit::platform::LoadAppSettings();
    EXPECT_EQ(persisted.fileListColumnWidths.at(L"BrowserContent.Name"), 222);
    EXPECT_EQ(persisted.searchListColumnWidths.at(L"SearchResults.Path"), widths[3]);

    dialog.Close();
    PumpMessages();
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();
    searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    ASSERT_NE(results, nullptr);
    for (std::size_t index = 0; index < widths.size(); ++index) {
        EXPECT_EQ(ListView_GetColumnWidth(results, static_cast<int>(index)), widths[index]);
    }

    dialog.Close();
    PumpMessages();
}

TEST(SearchPaneHistory, PersistsLastTwentyAndNavigatesWithArrows) {
    AppSettingsGuard settingsGuard;
    auto settings = wit::platform::LoadAppSettings();
    settings.quickSearchHistory.clear();
    for (int index = 1; index <= 21; ++index) {
        wit::platform::RememberQuickSearchQuery(settings, L"query " + std::to_wstring(index));
    }
    ASSERT_TRUE(wit::platform::SaveAppSettings(settings));

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto edit = GetDlgItem(searchWindow, IDC_SEARCH_NAME);
    ASSERT_NE(edit, nullptr);

    ASSERT_TRUE(SetWindowTextW(edit, L"draft"));
    SendMessageW(edit, WM_KEYDOWN, VK_UP, 0);
    EXPECT_EQ(WindowText(edit), L"query 21");
    EXPECT_EQ(SelectionStart(edit), WindowText(edit).size());
    SendMessageW(edit, WM_KEYDOWN, VK_UP, 0);
    EXPECT_EQ(WindowText(edit), L"query 20");
    EXPECT_EQ(SelectionStart(edit), WindowText(edit).size());
    SendMessageW(edit, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(WindowText(edit), L"query 21");
    EXPECT_EQ(SelectionStart(edit), WindowText(edit).size());
    SendMessageW(edit, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(WindowText(edit), L"draft");
    EXPECT_EQ(SelectionStart(edit), WindowText(edit).size());
    SendMessageW(edit, EM_SETSEL, 2, 2);
    SendMessageW(edit, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(WindowText(edit), L"draft");
    EXPECT_EQ(SelectionStart(edit), 2u);

    ASSERT_TRUE(SetWindowTextW(edit, L"query 22"));
    SendMessageW(GetDlgItem(searchWindow, IDC_SEARCH_EXECUTE), BM_CLICK, 0, 0);
    PumpMessages();

    const auto saved = wit::platform::LoadAppSettings();
    ASSERT_EQ(saved.quickSearchHistory.size(), 20u);
    EXPECT_EQ(saved.quickSearchHistory.front(), L"query 22");
    EXPECT_EQ(std::ranges::find(saved.quickSearchHistory, L"query 1"), saved.quickSearchHistory.end());
    EXPECT_EQ(std::ranges::find(saved.quickSearchHistory, L"query 2"), saved.quickSearchHistory.end());

    dialog.Close();
    PumpMessages();
}

TEST(SearchPaneStatus, ShowsCountFocusedSelectionAndElapsedTime) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    const auto status = GetDlgItem(searchWindow, IDC_SEARCH_STATUS);
    ASSERT_NE(results, nullptr);
    ASSERT_NE(status, nullptr);

    ASSERT_TRUE(SetDlgItemTextW(searchWindow, IDC_SEARCH_NAME, L"*"));
    SendMessageW(GetDlgItem(searchWindow, IDC_SEARCH_EXECUTE), BM_CLICK, 0, 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(results) != 1 && std::chrono::steady_clock::now() < deadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(results), 1);

    ListView_SetItemState(results, 0, LVIS_SELECTED | LVIS_FOCUSED,
        LVIS_SELECTED | LVIS_FOCUSED);
    PumpMessages();

    EXPECT_EQ(StatusPartText(status, 0), L"Items on list: 1");
    EXPECT_NE(StatusPartText(status, 1).find(L"replacement.txt, 700 MB"), std::wstring::npos);
    EXPECT_EQ(StatusPartText(status, 2), L"Selected items: 1 (total 700 MB)");
    const auto elapsed = StatusPartText(status, 3);
    EXPECT_TRUE(elapsed.ends_with(L" s"));

    dialog.Close();
    PumpMessages();
}
TEST(DISABLED_SearchPaneUiPerformance, SearchAndScrollFakeCatalog) {
    const auto catalogPath = std::filesystem::current_path() / L"tools" / L"catalog-test" /
        L"fake-search-catalog.sqlite";
    if (!std::filesystem::exists(catalogPath)) {
        // See tools/catalog-test/README.md for fixture generation instructions.
        GTEST_SKIP() << "tools\\catalog-test\\fake-search-catalog.sqlite is not available";
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    wit::storage::Database database;
    ASSERT_TRUE(database.OpenExisting(catalogPath.wstring()));

    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &database.SearchRepository(),
        [](const wit::core::FileEntry&) { return true; }, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto edit = GetDlgItem(searchWindow, IDC_SEARCH_NAME);
    const auto execute = GetDlgItem(searchWindow, IDC_SEARCH_EXECUTE);
    const auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    ASSERT_NE(edit, nullptr);
    ASSERT_NE(execute, nullptr);
    ASSERT_NE(results, nullptr);

    ASSERT_TRUE(SetWindowTextW(edit, L"file"));
    const double searchDispatchMs = MeasureMilliseconds([&] {
        SendMessageW(execute, BM_CLICK, 0, 0);
    });
    EXPECT_LT(searchDispatchMs, 250.0) << "Search preparation must not block the UI thread";

    const auto completionStarted = std::chrono::steady_clock::now();
    int itemCount{};
    while (itemCount == 0 &&
        std::chrono::steady_clock::now() - completionStarted < std::chrono::seconds(30)) {
        PumpMessages();
        Sleep(10);
        itemCount = static_cast<int>(SendMessageW(results, LVM_GETITEMCOUNT, 0, 0));
    }
    const double searchCompletionMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - completionStarted).count();
    ASSERT_EQ(itemCount, 500000);

    const int pageIterations = 200;
    const double pageMs = MeasureMilliseconds([&] {
        for (int index = 0; index < pageIterations; ++index) {
            SendMessageW(results, WM_VSCROLL, SB_PAGEDOWN, 0);
            UpdateWindow(results);
        }
    });

    SendMessageW(results, LVM_ENSUREVISIBLE, 0, FALSE);
    UpdateWindow(results);

    const int lineIterations = 1000;
    const double lineMs = MeasureMilliseconds([&] {
        for (int index = 0; index < lineIterations; ++index) {
            SendMessageW(results, WM_VSCROLL, SB_LINEDOWN, 0);
            UpdateWindow(results);
        }
    });

    const int jumpIterations = 400;
    const double jumpMs = MeasureMilliseconds([&] {
        for (int index = 0; index < jumpIterations; ++index) {
            const auto item = (std::min)(itemCount - 1, index * 1024);
            SendMessageW(results, LVM_ENSUREVISIBLE, item, FALSE);
            UpdateWindow(results);
        }
    });

    std::cout << "SCROLL_PERF search_term=file"
        << " result_count=" << itemCount
        << " search_dispatch_ms=" << searchDispatchMs
        << " search_completion_ms=" << searchCompletionMs
        << " page_down_iterations=" << pageIterations
        << " page_down_total_ms=" << pageMs
        << " page_down_avg_ms=" << pageMs / pageIterations
        << " line_down_iterations=" << lineIterations
        << " line_down_total_ms=" << lineMs
        << " line_down_avg_ms=" << lineMs / lineIterations
        << " jump_iterations=" << jumpIterations
        << " jump_total_ms=" << jumpMs
        << " jump_avg_ms=" << jumpMs / jumpIterations
        << std::endl;

    dialog.Close();
    PumpMessages();
    database.Close();
}
