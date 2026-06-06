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
#include <utility>
#include <vector>
#include <wit_gui/GeneralSettingsDialog.h>

extern WTL::CAppModule _Module;

namespace {

class WtlModuleGuard {
public:
    WtlModuleGuard() {
        std::call_once(initOnce_, [] {
            INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES};
            InitCommonControlsEx(&controls);
            initialized_ = SUCCEEDED(_Module.Init(nullptr, GetModuleHandleW(nullptr)));
        });
    }

    WtlModuleGuard(const WtlModuleGuard&) = delete;
    WtlModuleGuard& operator=(const WtlModuleGuard&) = delete;

    bool Initialized() const { return initialized_; }

private:
    inline static std::once_flag initOnce_;
    inline static bool initialized_{};
};

class DialogRun {
public:
    explicit DialogRun(wit::platform::AppSettings initial) : initial_(std::move(initial)) {}

    void Start() {
        startedFuture_ = started_.get_future();
        finishedFuture_ = finished_.get_future();
        thread_ = std::thread([this]() {
            threadId = GetCurrentThreadId();
            started_.set_value();
            wit::ui::GeneralSettingsDialog dialog;
            const bool result = dialog.Show(nullptr, initial_, accepted,
                [this](const wit::platform::AppSettings& settings) {
                    std::lock_guard lock(mutex);
                    appliedSettings.push_back(settings);
                    return true;
                });
            finished_.set_value(result);
        });
    }

    ~DialogRun() {
        if (thread_.joinable()) thread_.join();
    }

    std::future_status WaitStarted(std::chrono::seconds timeout) {
        return startedFuture_.wait_for(timeout);
    }

    std::future_status WaitFinished(std::chrono::seconds timeout) {
        return finishedFuture_.wait_for(timeout);
    }

    bool FinishedResult() {
        return finishedFuture_.get();
    }

    void Join() {
        if (thread_.joinable()) thread_.join();
    }

    std::mutex mutex;
    std::vector<wit::platform::AppSettings> appliedSettings;
    wit::platform::AppSettings accepted;
    DWORD threadId{};

private:
    wit::platform::AppSettings initial_;
    std::promise<void> started_;
    std::promise<bool> finished_;
    std::future<void> startedFuture_;
    std::future<bool> finishedFuture_;
    std::thread thread_;
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

wit::platform::AppSettings TestSettings() {
    wit::platform::AppSettings settings;
    settings.showStatusBar = true;
    settings.showToolbar = false;
    settings.enableScanFileDelay = false;
    settings.mainSplitterPosition = 360;
    settings.lastCatalogPath = L"H:\\github\\whereisthat\\tools\\import\\d-import-test.db";
    return settings;
}

}

TEST(GeneralSettingsDialog, NativeSettingsUiExposesOnlyRequestedPagesAndEditableSettings) {
    WtlModuleGuard module;
    ASSERT_TRUE(module.Initialized());

    const auto initial = TestSettings();
    DialogRun run(initial);
    run.Start();

    ASSERT_EQ(run.WaitStarted(std::chrono::seconds(5)), std::future_status::ready);
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
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_SETTINGS_PANEL)));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_SETTINGS_HEADER)));
    EXPECT_NE(GetWindowLongPtrW(GetDlgItem(dialog, IDC_LAST_OPENED_CATALOG), GWL_STYLE) & ES_READONLY, 0);
    EXPECT_NE(GetWindowLongPtrW(GetDlgItem(dialog, IDC_MAIN_SPLITTER_POSITION), GWL_STYLE) & ES_READONLY, 0);

    const HWND dateFormat = GetDlgItem(dialog, IDC_DATE_TIME_FORMAT);
    ASSERT_NE(dateFormat, nullptr);
    SendMessageW(dateFormat, CB_SETCURSEL, 1, 0);
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_DATE_TIME_FORMAT, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(dateFormat));
    EXPECT_EQ(WindowText(dateFormat), L"YYYY-MM-DD HH:mm:ss");
    EXPECT_TRUE(IsApplyEnabled(dialog));

    ClickButton(dialog, IDC_ENABLE_SCAN_FILE_DELAY);
    EXPECT_TRUE(IsApplyEnabled(dialog));

    TreeView_SelectItem(tree, userInterface);
    EXPECT_FALSE(IsWindowVisible(GetDlgItem(dialog, IDC_DATE_TIME_FORMAT)));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_SHOW_STATUS_BAR)));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_SHOW_TOOLBAR)));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_MAIN_SPLITTER_POSITION)));

    ClickButton(dialog, IDC_SHOW_STATUS_BAR);
    ClickButton(dialog, IDC_SHOW_TOOLBAR);
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
    ASSERT_EQ(run.WaitFinished(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_FALSE(run.FinishedResult());
    run.Join();
}

TEST(GeneralSettingsDialog, ReadOnlyInformationalFieldsDoNotDirtyOrApply) {
    WtlModuleGuard module;
    ASSERT_TRUE(module.Initialized());

    const auto initial = TestSettings();
    DialogRun run(initial);
    run.Start();

    ASSERT_EQ(run.WaitStarted(std::chrono::seconds(5)), std::future_status::ready);
    HWND dialog = WaitForSettingsWindow(run.threadId);
    ASSERT_NE(dialog, nullptr);

    SetWindowTextW(GetDlgItem(dialog, IDC_LAST_OPENED_CATALOG), L"changed.db");
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_LAST_OPENED_CATALOG, EN_CHANGE),
        reinterpret_cast<LPARAM>(GetDlgItem(dialog, IDC_LAST_OPENED_CATALOG)));
    SetWindowTextW(GetDlgItem(dialog, IDC_MAIN_SPLITTER_POSITION), L"not an integer");
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_MAIN_SPLITTER_POSITION, EN_CHANGE),
        reinterpret_cast<LPARAM>(GetDlgItem(dialog, IDC_MAIN_SPLITTER_POSITION)));

    EXPECT_FALSE(IsApplyEnabled(dialog));

    ClickButton(dialog, IDOK);
    ASSERT_EQ(run.WaitFinished(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_TRUE(run.FinishedResult());

    {
        std::lock_guard lock(run.mutex);
        EXPECT_TRUE(run.appliedSettings.empty());
    }
    EXPECT_EQ(run.accepted.mainSplitterPosition, initial.mainSplitterPosition);
    EXPECT_EQ(run.accepted.lastCatalogPath, initial.lastCatalogPath);
    run.Join();
}

TEST(GeneralSettingsDialog, OkAppliesPendingEditableChangesOnce) {
    WtlModuleGuard module;
    ASSERT_TRUE(module.Initialized());

    const auto initial = TestSettings();
    DialogRun run(initial);
    run.Start();

    ASSERT_EQ(run.WaitStarted(std::chrono::seconds(5)), std::future_status::ready);
    HWND dialog = WaitForSettingsWindow(run.threadId);
    ASSERT_NE(dialog, nullptr);

    ClickButton(dialog, IDC_ENABLE_SCAN_FILE_DELAY);
    EXPECT_TRUE(IsApplyEnabled(dialog));

    ClickButton(dialog, IDOK);
    ASSERT_EQ(run.WaitFinished(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_TRUE(run.FinishedResult());

    {
        std::lock_guard lock(run.mutex);
        ASSERT_EQ(run.appliedSettings.size(), 1u);
        EXPECT_TRUE(run.appliedSettings.back().enableScanFileDelay);
        EXPECT_EQ(run.appliedSettings.back().mainSplitterPosition, initial.mainSplitterPosition);
        EXPECT_EQ(run.appliedSettings.back().lastCatalogPath, initial.lastCatalogPath);
    }
    EXPECT_TRUE(run.accepted.enableScanFileDelay);
    run.Join();
}
