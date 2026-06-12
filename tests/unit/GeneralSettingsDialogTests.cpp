#include <gtest/gtest.h>

#include <Windows.h>
#include <CommCtrl.h>
#include <atlbase.h>
#include <atlapp.h>
#include <chrono>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <wit_gui/GeneralSettingsDialog.h>

extern WTL::CAppModule _Module;

namespace {

constexpr UINT kRunDialogActionMessage = WM_APP + 31;

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
            MSG probe{};
            PeekMessageW(&probe, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
            started_.set_value();
            wit::ui::GeneralSettingsDialog dialog;
            if (dialog.Show(nullptr, initial_, wit::ui::GeneralSettingsDialog::Page::General,
                [this](const wit::platform::AppSettings& settings) { return Apply(settings); })) {
                const HWND dialogWindow = dialog.WindowHandle();
                MSG message{};
                while (IsWindow(dialogWindow) && GetMessageW(&message, nullptr, 0, 0) > 0) {
                    if (message.message == kRunDialogActionMessage) {
                        RunNextAction(dialog);
                        continue;
                    }
                    if (!dialog.PreTranslateMessage(&message)) {
                        TranslateMessage(&message);
                        DispatchMessageW(&message);
                    }
                }
            }
            finished_.set_value();
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

    template <typename Action>
    void Invoke(Action action) {
        auto complete = std::make_shared<std::promise<void>>();
        auto finished = complete->get_future();
        {
            std::lock_guard lock(actionsMutex_);
            actions_.emplace_back([action = std::move(action), complete](wit::ui::GeneralSettingsDialog& dialog) {
                action(dialog);
                complete->set_value();
            });
        }
        PostThreadMessageW(threadId, kRunDialogActionMessage, 0, 0);
        EXPECT_EQ(finished.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    }

    void ShowAgain(const wit::platform::AppSettings& settings, wit::ui::GeneralSettingsDialog::Page page) {
        Invoke([this, settings, page](wit::ui::GeneralSettingsDialog& dialog) {
            EXPECT_TRUE(dialog.Show(nullptr, settings, page,
                [this](const wit::platform::AppSettings& applied) { return Apply(applied); }));
        });
    }

    void Join() {
        if (thread_.joinable()) thread_.join();
    }

    void WakeMessageLoop() const {
        PostThreadMessageW(threadId, WM_NULL, 0, 0);
    }

    std::mutex mutex;
    std::vector<wit::platform::AppSettings> appliedSettings;
    DWORD threadId{};

private:
    wit::ui::GeneralSettingsDialog::ApplyResult Apply(const wit::platform::AppSettings& settings) {
        std::lock_guard lock(mutex);
        appliedSettings.push_back(settings);
        return wit::ui::GeneralSettingsDialog::ApplyResult::Applied;
    }

    void RunNextAction(wit::ui::GeneralSettingsDialog& dialog) {
        std::function<void(wit::ui::GeneralSettingsDialog&)> action;
        {
            std::lock_guard lock(actionsMutex_);
            if (actions_.empty()) return;
            action = std::move(actions_.front());
            actions_.pop_front();
        }
        action(dialog);
    }

    wit::platform::AppSettings initial_;
    std::promise<void> started_;
    std::promise<void> finished_;
    std::future<void> startedFuture_;
    std::future<void> finishedFuture_;
    std::thread thread_;
    std::mutex actionsMutex_;
    std::deque<std::function<void(wit::ui::GeneralSettingsDialog&)>> actions_;
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

struct FindControlContext {
    int id{};
    HWND control{};
};

BOOL CALLBACK FindDescendantControl(HWND window, LPARAM context) {
    auto& search = *reinterpret_cast<FindControlContext*>(context);
    if (GetDlgCtrlID(window) == search.id) {
        search.control = window;
        return FALSE;
    }
    return TRUE;
}

HWND Control(HWND dialog, int controlId) {
    if (const auto control = GetDlgItem(dialog, controlId)) return control;
    FindControlContext context{controlId};
    EnumChildWindows(dialog, FindDescendantControl, reinterpret_cast<LPARAM>(&context));
    return context.control;
}

void ClickButton(HWND dialog, int controlId) {
    SendMessageW(Control(dialog, controlId), BM_CLICK, 0, 0);
}

bool IsApplyEnabled(HWND dialog) {
    return IsWindowEnabled(Control(dialog, IDC_SETTINGS_APPLY)) != FALSE;
}

wit::platform::AppSettings TestSettings() {
    wit::platform::AppSettings settings;
    settings.showStatusBar = true;
    settings.showToolbar = false;
    settings.doNotShowAlphaWarning = false;
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
    const HTREEITEM fileList = TreeView_GetNextSibling(tree, userInterface);
    ASSERT_NE(fileList, nullptr);
    const HTREEITEM diskImage = TreeView_GetNextSibling(tree, fileList);
    ASSERT_NE(diskImage, nullptr);
    const HTREEITEM description = TreeView_GetNextSibling(tree, diskImage);
    ASSERT_NE(description, nullptr);
    EXPECT_EQ(TreeView_GetNextSibling(tree, description), nullptr);
    EXPECT_EQ(TreeView_GetChild(tree, general), nullptr);
    EXPECT_EQ(TreeView_GetChild(tree, userInterface), nullptr);
    EXPECT_EQ(TreeView_GetChild(tree, fileList), nullptr);
    EXPECT_EQ(TreeView_GetChild(tree, diskImage), nullptr);
    EXPECT_EQ(TreeView_GetChild(tree, description), nullptr);
    EXPECT_EQ(TreeItemText(tree, general), L"General Settings");
    EXPECT_EQ(TreeItemText(tree, userInterface), L"User Interface Setup");
    EXPECT_EQ(TreeItemText(tree, fileList), L"File List Settings");
    EXPECT_EQ(TreeItemText(tree, diskImage), L"Disk Image Settings");
    EXPECT_EQ(TreeItemText(tree, description), L"Description Settings");

    EXPECT_FALSE(IsApplyEnabled(dialog));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_SETTINGS_PANEL)));
    EXPECT_TRUE(IsWindowVisible(GetDlgItem(dialog, IDC_SETTINGS_HEADER)));
    EXPECT_NE(GetWindowLongPtrW(Control(dialog, IDC_LAST_OPENED_CATALOG), GWL_STYLE) & ES_READONLY, 0);
    EXPECT_NE(GetWindowLongPtrW(Control(dialog, IDC_MAIN_SPLITTER_POSITION), GWL_STYLE) & ES_READONLY, 0);

    const HWND dateFormat = Control(dialog, IDC_DATE_TIME_FORMAT);
    ASSERT_NE(dateFormat, nullptr);
    SendMessageW(dateFormat, CB_SETCURSEL, 1, 0);
    SendMessageW(GetParent(dateFormat), WM_COMMAND, MAKEWPARAM(IDC_DATE_TIME_FORMAT, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(dateFormat));
    EXPECT_EQ(WindowText(dateFormat), L"YYYY-MM-DD HH:mm:ss");
    EXPECT_TRUE(IsApplyEnabled(dialog));

    ClickButton(dialog, IDC_ENABLE_SCAN_FILE_DELAY);
    EXPECT_TRUE(IsApplyEnabled(dialog));

    TreeView_SelectItem(tree, userInterface);
    EXPECT_FALSE(IsWindowVisible(Control(dialog, IDC_DATE_TIME_FORMAT)));
    EXPECT_TRUE(IsWindowVisible(Control(dialog, IDC_SHOW_STATUS_BAR)));
    EXPECT_TRUE(IsWindowVisible(Control(dialog, IDC_SHOW_TOOLBAR)));
    EXPECT_TRUE(IsWindowVisible(Control(dialog, IDC_DO_NOT_SHOW_ALPHA_WARNING)));
    EXPECT_TRUE(IsWindowVisible(Control(dialog, IDC_MAIN_SPLITTER_POSITION)));

    ClickButton(dialog, IDC_SHOW_STATUS_BAR);
    ClickButton(dialog, IDC_SHOW_TOOLBAR);
    ClickButton(dialog, IDC_DO_NOT_SHOW_ALPHA_WARNING);
    EXPECT_TRUE(IsApplyEnabled(dialog));

    ClickButton(dialog, IDC_SETTINGS_APPLY);
    EXPECT_FALSE(IsApplyEnabled(dialog));

    {
        std::lock_guard lock(run.mutex);
        ASSERT_EQ(run.appliedSettings.size(), 1u);
        const auto& applied = run.appliedSettings.back();
        EXPECT_FALSE(applied.showStatusBar);
        EXPECT_TRUE(applied.showToolbar);
        EXPECT_TRUE(applied.doNotShowAlphaWarning);
        EXPECT_TRUE(applied.enableScanFileDelay);
        EXPECT_EQ(applied.dateTimeFormat, L"YYYY-MM-DD HH:mm:ss");
        EXPECT_EQ(applied.mainSplitterPosition, 360);
        EXPECT_EQ(applied.lastCatalogPath, initial.lastCatalogPath);
    }

    SendMessageW(dialog, WM_CLOSE, 0, 0);
    run.WakeMessageLoop();
    ASSERT_EQ(run.WaitFinished(std::chrono::seconds(5)), std::future_status::ready);
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

    SetWindowTextW(Control(dialog, IDC_LAST_OPENED_CATALOG), L"changed.db");
    SendMessageW(GetParent(Control(dialog, IDC_LAST_OPENED_CATALOG)), WM_COMMAND,
        MAKEWPARAM(IDC_LAST_OPENED_CATALOG, EN_CHANGE),
        reinterpret_cast<LPARAM>(Control(dialog, IDC_LAST_OPENED_CATALOG)));
    SetWindowTextW(Control(dialog, IDC_MAIN_SPLITTER_POSITION), L"not an integer");
    SendMessageW(GetParent(Control(dialog, IDC_MAIN_SPLITTER_POSITION)), WM_COMMAND,
        MAKEWPARAM(IDC_MAIN_SPLITTER_POSITION, EN_CHANGE),
        reinterpret_cast<LPARAM>(Control(dialog, IDC_MAIN_SPLITTER_POSITION)));

    EXPECT_FALSE(IsApplyEnabled(dialog));

    ClickButton(dialog, IDOK);
    run.WakeMessageLoop();
    ASSERT_EQ(run.WaitFinished(std::chrono::seconds(5)), std::future_status::ready);

    {
        std::lock_guard lock(run.mutex);
        EXPECT_TRUE(run.appliedSettings.empty());
    }
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
    run.WakeMessageLoop();
    ASSERT_EQ(run.WaitFinished(std::chrono::seconds(5)), std::future_status::ready);

    {
        std::lock_guard lock(run.mutex);
        ASSERT_EQ(run.appliedSettings.size(), 1u);
        EXPECT_TRUE(run.appliedSettings.back().enableScanFileDelay);
        EXPECT_FALSE(run.appliedSettings.back().doNotShowAlphaWarning);
        EXPECT_EQ(run.appliedSettings.back().mainSplitterPosition, initial.mainSplitterPosition);
        EXPECT_EQ(run.appliedSettings.back().lastCatalogPath, initial.lastCatalogPath);
    }
    run.Join();
}

TEST(GeneralSettingsDialog, ReopeningModelessDialogRefreshesUntouchedExternalSettings) {
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

    auto externallyUpdated = initial;
    externallyUpdated.showStatusBar = false;
    externallyUpdated.showToolbar = true;
    externallyUpdated.doNotShowAlphaWarning = true;
    run.ShowAgain(externallyUpdated, wit::ui::GeneralSettingsDialog::Page::UserInterface);

    EXPECT_EQ(SendMessageW(Control(dialog, IDC_ENABLE_SCAN_FILE_DELAY), BM_GETCHECK, 0, 0), BST_CHECKED);
    EXPECT_EQ(SendMessageW(Control(dialog, IDC_SHOW_STATUS_BAR), BM_GETCHECK, 0, 0), BST_UNCHECKED);
    EXPECT_EQ(SendMessageW(Control(dialog, IDC_SHOW_TOOLBAR), BM_GETCHECK, 0, 0), BST_CHECKED);
    EXPECT_EQ(SendMessageW(Control(dialog, IDC_DO_NOT_SHOW_ALPHA_WARNING), BM_GETCHECK, 0, 0), BST_CHECKED);

    const HWND dateFormat = Control(dialog, IDC_DATE_TIME_FORMAT);
    ASSERT_NE(dateFormat, nullptr);
    SendMessageW(dateFormat, CB_SETCURSEL, 1, 0);
    SendMessageW(GetParent(dateFormat), WM_COMMAND, MAKEWPARAM(IDC_DATE_TIME_FORMAT, CBN_SELCHANGE),
        reinterpret_cast<LPARAM>(dateFormat));

    ClickButton(dialog, IDC_SETTINGS_APPLY);

    {
        std::lock_guard lock(run.mutex);
        ASSERT_EQ(run.appliedSettings.size(), 1u);
        const auto& applied = run.appliedSettings.back();
        EXPECT_FALSE(applied.showStatusBar);
        EXPECT_TRUE(applied.showToolbar);
        EXPECT_TRUE(applied.doNotShowAlphaWarning);
        EXPECT_TRUE(applied.enableScanFileDelay);
        EXPECT_EQ(applied.dateTimeFormat, L"YYYY-MM-DD HH:mm:ss");
    }

    SendMessageW(dialog, WM_CLOSE, 0, 0);
    run.WakeMessageLoop();
    ASSERT_EQ(run.WaitFinished(std::chrono::seconds(5)), std::future_status::ready);
    run.Join();
}
