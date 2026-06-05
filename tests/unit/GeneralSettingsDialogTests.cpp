#include <gtest/gtest.h>

#include <Windows.h>
#include <CommCtrl.h>
#include <atlbase.h>
#include <atlapp.h>
#include <chrono>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <wit_gui/GeneralSettingsDialog.h>

extern WTL::CAppModule _Module;

namespace {

class WtlModuleGuard {
public:
    WtlModuleGuard() {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&controls);
        initialized_ = SUCCEEDED(_Module.Init(nullptr, GetModuleHandleW(nullptr)));
    }

    ~WtlModuleGuard() {
        if (initialized_) _Module.Term();
    }

    WtlModuleGuard(const WtlModuleGuard&) = delete;
    WtlModuleGuard& operator=(const WtlModuleGuard&) = delete;

    bool Initialized() const { return initialized_; }

private:
    bool initialized_{};
};

struct DialogRun {
    std::promise<void> started;
    std::promise<bool> finished;
    std::mutex mutex;
    std::vector<wit::platform::AppSettings> appliedSettings;
    DWORD threadId{};
};

BOOL CALLBACK FindThreadWindow(HWND window, LPARAM context) {
    wchar_t title[64]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    if (wcscmp(title, L"Settings") == 0) {
        *reinterpret_cast<HWND*>(context) = window;
        return FALSE;
    }
    return TRUE;
}

void AssertClickableAndClick(HWND dialog, HWND window) {
    RECT rect{};
    ASSERT_TRUE(GetWindowRect(window, &rect));
    POINT screenPoint{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    EXPECT_EQ(WindowFromPoint(screenPoint), window);
    SendMessageW(window, BM_CLICK, 0, 0);
}

HWND WaitForSettingsWindow(DWORD threadId) {
    const auto deadline = GetTickCount64() + 5000;
    while (GetTickCount64() < deadline) {
        HWND window{};
        EnumThreadWindows(threadId, FindThreadWindow, reinterpret_cast<LPARAM>(&window));
        if (window) return window;
        Sleep(10);
    }
    return nullptr;
}

std::wstring WindowText(HWND window) {
    const int length = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

std::wstring TreeItemText(HWND tree, HTREEITEM item) {
    wchar_t buffer[128]{};
    TVITEMW treeItem{};
    treeItem.mask = TVIF_TEXT;
    treeItem.hItem = item;
    treeItem.pszText = buffer;
    treeItem.cchTextMax = static_cast<int>(std::size(buffer));
    TreeView_GetItem(tree, &treeItem);
    return buffer;
}

void ClickButton(HWND dialog, int controlId) {
    SendMessageW(GetDlgItem(dialog, controlId), BM_CLICK, 0, 0);
}

bool IsApplyEnabled(HWND dialog) {
    return IsWindowEnabled(GetDlgItem(dialog, IDC_SETTINGS_APPLY)) != FALSE;
}

}

TEST(GeneralSettingsDialog, NativeSettingsUiExposesOnlyRequestedPagesAndEditableSettings) {
    WtlModuleGuard module;
    ASSERT_TRUE(module.Initialized());

    wit::platform::AppSettings initial;
    initial.showStatusBar = true;
    initial.showToolbar = false;
    initial.enableScanFileDelay = false;
    initial.mainSplitterPosition = 360;
    initial.lastCatalogPath = L"H:\\github\\whereisthat\\tools\\import\\d-import-test.db";

    DialogRun run;
    std::future<void> started = run.started.get_future();
    std::future<bool> finished = run.finished.get_future();

    std::thread dialogThread([&run, initial]() mutable {
        run.threadId = GetCurrentThreadId();
        run.started.set_value();
        wit::ui::GeneralSettingsDialog dialog;
        wit::platform::AppSettings accepted;
        const bool result = dialog.Show(nullptr, initial, accepted,
            [&run](const wit::platform::AppSettings& settings) {
                std::lock_guard lock(run.mutex);
                run.appliedSettings.push_back(settings);
                return true;
            });
        run.finished.set_value(result);
    });

    ASSERT_EQ(started.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    HWND dialog = WaitForSettingsWindow(run.threadId);
    ASSERT_NE(dialog, nullptr);

    RECT windowRect{};
    GetWindowRect(dialog, &windowRect);
    EXPECT_LE(windowRect.right - windowRect.left, 760) << "settings dialog should not open oversized";
    EXPECT_LE(windowRect.bottom - windowRect.top, 560) << "settings dialog should not open oversized";

    const HWND tree = GetDlgItem(dialog, IDC_SETTINGS_TREE);
    ASSERT_NE(tree, nullptr);
    const HTREEITEM general = TreeView_GetRoot(tree);
    ASSERT_NE(general, nullptr);
    const HTREEITEM userInterface = TreeView_GetNextSibling(tree, general);
    ASSERT_NE(userInterface, nullptr);
    EXPECT_EQ(TreeView_GetNextSibling(tree, userInterface), nullptr);
    EXPECT_EQ(TreeView_GetChild(tree, general), nullptr);
    EXPECT_EQ(TreeView_GetChild(tree, userInterface), nullptr);
    EXPECT_EQ(TreeItemText(tree, general), L"General Settings");
    EXPECT_EQ(TreeItemText(tree, userInterface), L"User Interface Setup");

    EXPECT_FALSE(IsApplyEnabled(dialog));
    EXPECT_FALSE(IsWindowVisible(GetDlgItem(dialog, IDC_SETTINGS_PANEL)));
    EXPECT_FALSE(IsWindowVisible(GetDlgItem(dialog, IDC_SETTINGS_HEADER)));
    EXPECT_NE(GetWindowLongPtrW(GetDlgItem(dialog, IDC_LAST_OPENED_CATALOG), GWL_STYLE) & ES_READONLY, 0);
    EXPECT_NE(GetWindowLongPtrW(GetDlgItem(dialog, IDC_MAIN_SPLITTER_POSITION), GWL_STYLE) & ES_READONLY, 0);

    const HWND dateFormat = GetDlgItem(dialog, IDC_DATE_TIME_FORMAT);
    ASSERT_NE(dateFormat, nullptr);
    SendMessageW(dateFormat, CB_SETCURSEL, 1, 0);
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_DATE_TIME_FORMAT, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(dateFormat));
    EXPECT_EQ(WindowText(dateFormat), L"YYYY-MM-DD HH:mm:ss");
    EXPECT_TRUE(IsApplyEnabled(dialog));

    AssertClickableAndClick(dialog, GetDlgItem(dialog, IDC_ENABLE_SCAN_FILE_DELAY));
    EXPECT_TRUE(IsApplyEnabled(dialog));

    TreeView_SelectItem(tree, userInterface);
    EXPECT_FALSE(IsWindowVisible(GetDlgItem(dialog, IDC_DATE_TIME_FORMAT)));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_SHOW_STATUS_BAR)));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_SHOW_TOOLBAR)));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_MAIN_SPLITTER_POSITION)));

    AssertClickableAndClick(dialog, GetDlgItem(dialog, IDC_SHOW_STATUS_BAR));
    AssertClickableAndClick(dialog, GetDlgItem(dialog, IDC_SHOW_TOOLBAR));
    EXPECT_TRUE(IsApplyEnabled(dialog));

    ClickButton(dialog, IDC_SETTINGS_APPLY);
    EXPECT_FALSE(IsApplyEnabled(dialog));

    {
        std::lock_guard lock(run.mutex);
        ASSERT_EQ(run.appliedSettings.size(), 1u);
        const auto& applied = run.appliedSettings.back();
        EXPECT_FALSE(applied.showStatusBar);
        EXPECT_TRUE(applied.showToolbar);
        EXPECT_TRUE(applied.enableScanFileDelay);
        EXPECT_EQ(applied.dateTimeFormat, L"YYYY-MM-DD HH:mm:ss");
        EXPECT_EQ(applied.mainSplitterPosition, 360);
        EXPECT_EQ(applied.lastCatalogPath, initial.lastCatalogPath);
    }

    SendMessageW(dialog, WM_CLOSE, 0, 0);
    ASSERT_EQ(finished.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_FALSE(finished.get());
    dialogThread.join();
}
