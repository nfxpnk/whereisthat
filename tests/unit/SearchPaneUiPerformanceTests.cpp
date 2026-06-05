#include <gtest/gtest.h>
#include <wit_database/Database.h>
#include <wit_gui/SearchPane.h>
#include <CommCtrl.h>
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <iostream>

WTL::CAppModule _Module;

namespace {
class AtlModuleGuard {
public:
    AtlModuleGuard() {
        initialized_ = SUCCEEDED(_Module.Init(nullptr, GetModuleHandleW(nullptr)));
    }

    ~AtlModuleGuard() {
        if (initialized_) _Module.Term();
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

template <typename Func>
double MeasureMilliseconds(Func&& func) {
    const auto started = std::chrono::steady_clock::now();
    func();
    PumpMessages();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return std::chrono::duration<double, std::milli>(elapsed).count();
}
}

TEST(DISABLED_SearchPaneUiPerformance, SearchAndScrollFakeCatalog) {
    const auto catalogPath = std::filesystem::current_path() / L"tools" / L"catalog-test" /
        L"fake-search-catalog.sqlite";
    if (!std::filesystem::exists(catalogPath)) {
        GTEST_SKIP() << "tools\\catalog-test\\fake-search-catalog.sqlite is not available";
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    wit::storage::Database database;
    ASSERT_TRUE(database.OpenExisting(catalogPath.wstring()));

    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &database.SearchRepository(), [] {}));
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
    const double searchMs = MeasureMilliseconds([&] {
        SendMessageW(execute, BM_CLICK, 0, 0);
    });

    const auto itemCount = static_cast<int>(SendMessageW(results, LVM_GETITEMCOUNT, 0, 0));
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
        << " search_ms=" << searchMs
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
