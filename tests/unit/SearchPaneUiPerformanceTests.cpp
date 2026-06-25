#include <gtest/gtest.h>
#include <wit_database/Database.h>
#include <wit_gui/BrowserItemIcons.h>
#include <wit_gui/FileListPane.h>
#include <wit_gui/SearchPane.h>
#include <CommCtrl.h>
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

WTL::CAppModule _Module;

namespace {
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
        const std::wstring& nameTerm, int limit, wit::core::FileSort sort) override {
        wit::search::PreparedSearchResult result;
        result.total = CountByName(nameTerm);
        if (result.total > 0) result.entries = PageByName(nameTerm, 0, limit, sort);
        return result;
    }

    wit::search::PreparedSearchResult PrepareAdvanced(
        const wit::search::AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort) override {
        wit::search::PreparedSearchResult result;
        result.total = CountAdvanced(expression);
        if (result.total > 0) result.entries = PageAdvanced(expression, 0, limit, sort);
        return result;
    }

    int CountByName(const std::wstring&) override {
        countStarted = true;
        while (!cancelled.load()) std::this_thread::yield();
        return 0;
    }

    std::vector<wit::core::FileEntry> PageByName(
        const std::wstring&, int, int, wit::core::FileSort) override {
        return {};
    }

    int CountAdvanced(const wit::search::AdvancedSearchExpression&) override {
        return CountByName({});
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
        const std::wstring& nameTerm, int limit, wit::core::FileSort sort) override {
        wit::search::PreparedSearchResult result;
        result.total = CountByName(nameTerm);
        if (result.total > 0) result.entries = PageByName(nameTerm, 0, limit, sort);
        return result;
    }

    wit::search::PreparedSearchResult PrepareAdvanced(
        const wit::search::AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort) override {
        wit::search::PreparedSearchResult result;
        result.total = CountAdvanced(expression);
        if (result.total > 0) result.entries = PageAdvanced(expression, 0, limit, sort);
        return result;
    }

    int CountByName(const std::wstring&) override { return 1; }

    std::vector<wit::core::FileEntry> PageByName(
        const std::wstring&, int, int, wit::core::FileSort) override {
        wit::core::FileEntry entry;
        entry.name = L"replacement.txt";
        entry.extension = L"txt";
        return {std::move(entry)};
    }

    int CountAdvanced(const wit::search::AdvancedSearchExpression&) override { return 1; }

    std::vector<wit::core::FileEntry> PageAdvanced(
        const wit::search::AdvancedSearchExpression&, int, int, wit::core::FileSort) override {
        return PageByName({}, 0, 1, {});
    }

    void CancelPending() override {}
    std::wstring LastErrorMessage() const override { return {}; }
};

template <typename Func>
double MeasureMilliseconds(Func&& func) {
    const auto started = std::chrono::steady_clock::now();
    func();
    PumpMessages();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return std::chrono::duration<double, std::milli>(elapsed).count();
}
}

TEST(SearchPaneIcons, UsesBrowserFileListIconRules) {
    wit::core::FileEntry folder;
    folder.isDirectory = true;
    EXPECT_EQ(wit::ui::ImageForFileEntry(folder), wit::ui::BrowserFolderImage);

    folder.isArchive = true;
    EXPECT_EQ(wit::ui::ImageForFileEntry(folder), wit::ui::BrowserArchiveImage);

    wit::core::FileEntry textFile;
    textFile.extension = L"txt";
    EXPECT_EQ(wit::ui::ImageForFileEntry(textFile), wit::ui::BrowserFileTxtImage);

    wit::core::FileEntry archiveFile;
    archiveFile.extension = L"arj";
    EXPECT_EQ(wit::ui::ImageForFileEntry(archiveFile), wit::ui::BrowserArchiveImage);

    wit::core::FileEntry unknownFile;
    unknownFile.extension = L"unknown";
    EXPECT_EQ(wit::ui::ImageForFileEntry(unknownFile), wit::ui::BrowserDocumentImage);
}

TEST(SearchPaneLifetime, RebindingCancelsTheOldRepositoryBeforeReplacement) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
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

TEST(DISABLED_SearchPaneUiPerformance, SearchAndScrollFakeCatalog) {
    const auto catalogPath = std::filesystem::current_path() / L"tools" / L"catalog-test" /
        L"fake-search-catalog.sqlite";
    if (!std::filesystem::exists(catalogPath)) {
        // See tools/catalog-test/README.md for fixture generation instructions.
        GTEST_SKIP() << "tools\\catalog-test\\fake-search-catalog.sqlite is not available";
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
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
