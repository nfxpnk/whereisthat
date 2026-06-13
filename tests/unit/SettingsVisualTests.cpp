#include <gtest/gtest.h>

#include <Windows.h>
#include <CommCtrl.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "resource.h"

// These tests require an interactive desktop session.
// In CI they are skipped automatically via IsWindowVisible(GetDesktopWindow()) guard.

namespace {

struct WindowSearch {
    DWORD processId{};
    const wchar_t* className{};
    const wchar_t* title{};
    HWND found{};
};

struct BgraPixel {
    std::uint8_t b{};
    std::uint8_t g{};
    std::uint8_t r{};
    std::uint8_t a{};
};

struct CapturedBitmap {
    int width{};
    int height{};
    std::vector<BgraPixel> pixels;
};

struct FindControlContext {
    int id{};
    HWND control{};
};

BOOL CALLBACK FindWindowForProcess(HWND window, LPARAM context) {
    auto& search = *reinterpret_cast<WindowSearch*>(context);
    DWORD processId{};
    GetWindowThreadProcessId(window, &processId);
    if (processId != search.processId || !IsWindowVisible(window)) return TRUE;

    if (search.className) {
        wchar_t className[128]{};
        GetClassNameW(window, className, static_cast<int>(std::size(className)));
        if (wcscmp(className, search.className) != 0) return TRUE;
    }

    if (search.title) {
        wchar_t title[128]{};
        GetWindowTextW(window, title, static_cast<int>(std::size(title)));
        if (wcscmp(title, search.title) != 0) return TRUE;
    }

    search.found = window;
    return FALSE;
}

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

HWND WaitForWindow(DWORD processId, const wchar_t* className, const wchar_t* title) {
    const auto deadline = GetTickCount64() + 10000;
    while (GetTickCount64() < deadline) {
        WindowSearch search{processId, className, title};
        EnumWindows(FindWindowForProcess, reinterpret_cast<LPARAM>(&search));
        if (search.found) return search.found;
        Sleep(50);
    }
    return nullptr;
}

HWND WaitForMainWindow(DWORD processId) {
    if (const HWND window = WaitForWindow(processId, L"WhereIsThatMainFrame", L"Where Is That?")) return window;
    if (const HWND window = WaitForWindow(processId, nullptr, L"Where Is That?")) return window;
    return WaitForWindow(processId, L"WhereIsThatMainFrame", nullptr);
}

std::filesystem::path TestExecutableDirectory() {
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

std::filesystem::path AppExecutablePath() {
    return TestExecutableDirectory() / L"WhereIsThat.exe";
}

std::filesystem::path StartupCatalogPath() {
    std::vector<std::filesystem::path> starts{std::filesystem::current_path(), TestExecutableDirectory()};
    for (auto start : starts) {
        start = std::filesystem::absolute(start);
        while (!start.empty()) {
            const auto path = start / L"tools" / L"catalog-test" / L"fake-search-catalog.sqlite";
            if (std::filesystem::exists(path)) return path;
            if (start == start.root_path()) break;
            start = start.parent_path();
        }
    }
    return {};
}

std::filesystem::path ArtifactPath(const wchar_t* fileName) {
    const auto directory = std::filesystem::current_path() / L"test-artifacts";
    std::filesystem::create_directories(directory);
    return directory / fileName;
}

CapturedBitmap CaptureWindow(HWND window) {
    RECT rect{};
    if (!GetWindowRect(window, &rect)) return {};

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return {};

    HDC windowDc = GetWindowDC(window);
    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, width, height);
    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

    BOOL printed = PrintWindow(window, memoryDc, PW_CLIENTONLY);
    if (!printed) {
        BitBlt(memoryDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    CapturedBitmap capture;
    capture.width = width;
    capture.height = height;
    capture.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    GetDIBits(memoryDc, bitmap, 0, static_cast<UINT>(height), capture.pixels.data(), &info, DIB_RGB_COLORS);

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(window, windowDc);
    return capture;
}

bool SaveBmp(const CapturedBitmap& capture, const std::filesystem::path& path) {
    if (capture.width <= 0 || capture.height <= 0 || capture.pixels.empty()) return false;

    BITMAPFILEHEADER fileHeader{};
    BITMAPINFOHEADER infoHeader{};
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = capture.width;
    infoHeader.biHeight = -capture.height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = static_cast<DWORD>(capture.pixels.size() * sizeof(BgraPixel));

    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + infoHeader.biSizeImage;

    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    output.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    output.write(reinterpret_cast<const char*>(capture.pixels.data()), infoHeader.biSizeImage);
    return output.good();
}

int DistinctColorBuckets(const CapturedBitmap& capture) {
    std::vector<std::uint32_t> buckets;
    buckets.reserve(capture.pixels.size());
    for (const auto& pixel : capture.pixels) {
        buckets.push_back(
            (static_cast<std::uint32_t>(pixel.r >> 4) << 8) |
            (static_cast<std::uint32_t>(pixel.g >> 4) << 4) |
            static_cast<std::uint32_t>(pixel.b >> 4));
    }
    std::ranges::sort(buckets);
    return static_cast<int>(std::ranges::unique(buckets).begin() - buckets.begin());
}

RECT ChildRect(HWND dialog, int controlId) {
    RECT rect{};
    GetWindowRect(Control(dialog, controlId), &rect);
    return rect;
}

bool RectInside(const RECT& outer, const RECT& inner) {
    return inner.left >= outer.left &&
        inner.top >= outer.top &&
        inner.right <= outer.right &&
        inner.bottom <= outer.bottom &&
        inner.right > inner.left &&
        inner.bottom > inner.top;
}

bool RectsOverlap(const RECT& left, const RECT& right) {
    RECT intersection{};
    return IntersectRect(&intersection, &left, &right) != FALSE;
}

std::wstring WindowText(HWND window) {
    const int length = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

bool WaitForControlText(HWND dialog, int controlId, const wchar_t* expected) {
    const auto deadline = GetTickCount64() + 5000;
    while (GetTickCount64() < deadline) {
        const HWND control = Control(dialog, controlId);
        if (control && WindowText(control) == expected) return true;
        Sleep(25);
    }
    return false;
}

std::string NarrowForTest(const std::wstring& text) {
    std::string result;
    result.reserve(text.size());
    for (const auto ch : text) {
        result.push_back(ch >= 0 && ch <= 127 ? static_cast<char>(ch) : '?');
    }
    return result;
}

bool TextFitsControl(HWND control, int horizontalPadding = 4) {
    const auto text = WindowText(control);
    if (text.empty()) return true;

    RECT rect{};
    if (!GetClientRect(control, &rect)) return false;

    HDC dc = GetDC(control);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    SIZE textSize{};
    const bool measured = GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &textSize) != FALSE;
    if (oldFont) SelectObject(dc, oldFont);
    ReleaseDC(control, dc);

    return measured && textSize.cx + horizontalPadding <= rect.right - rect.left;
}

class ProcessHandle {
public:
    explicit ProcessHandle(HANDLE handle = nullptr) : handle_(handle) {}
    ~ProcessHandle() {
        if (handle_) CloseHandle(handle_);
    }

    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;

    HANDLE Get() const { return handle_; }

private:
    HANDLE handle_{};
};

class SettingsFileGuard {
public:
    SettingsFileGuard() : path_(TestExecutableDirectory() / L"settings.ini") {
        if (std::filesystem::exists(path_)) {
            hadOriginal_ = true;
            std::ifstream input(path_, std::ios::binary);
            original_ = std::vector<char>(std::istreambuf_iterator<char>(input), {});
        }

        std::ofstream output(path_, std::ios::binary | std::ios::trunc);
        output << "[General]\r\n"
            << "ShowStatusBar=1\r\n"
            << "ShowToolbar=1\r\n"
            << "DoNotShowAlphaWarning=1\r\n"
            << "EnableScanFileDelay=0\r\n"
            << "MainSplitterPosition=360\r\n"
            << "DateTimeFormat=\r\n"
            << "LastCatalogPath=\r\n";
    }

    ~SettingsFileGuard() {
        if (!hadOriginal_) {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
            return;
        }

        std::ofstream output(path_, std::ios::binary | std::ios::trunc);
        output.write(original_.data(), static_cast<std::streamsize>(original_.size()));
    }

    SettingsFileGuard(const SettingsFileGuard&) = delete;
    SettingsFileGuard& operator=(const SettingsFileGuard&) = delete;

private:
    std::filesystem::path path_;
    std::vector<char> original_;
    bool hadOriginal_{};
};

class AppProcessGuard {
public:
    explicit AppProcessGuard(HANDLE process) : process_(process) {}
    ~AppProcessGuard() {
        if (process_ && WaitForSingleObject(process_, 2000) == WAIT_TIMEOUT) {
            TerminateProcess(process_, 0);
        }
    }

    AppProcessGuard(const AppProcessGuard&) = delete;
    AppProcessGuard& operator=(const AppProcessGuard&) = delete;

private:
    HANDLE process_{};
};

}

TEST(SettingsVisual, OpensAppSettingsAndCapturesScreenshots) {
    if (!IsWindowVisible(GetDesktopWindow())) {
        GTEST_SKIP() << "Skipped: no interactive desktop (headless CI)";
    }

    const auto appPath = AppExecutablePath();
    if (!std::filesystem::exists(appPath)) {
        GTEST_SKIP() << "Build WhereIsThat.exe before running this visual smoke test";
    }

    const auto startupCatalogPath = StartupCatalogPath();
    SettingsFileGuard settingsFile;
    std::wstring commandLine = startupCatalogPath.empty()
        ? std::format(L"\"{}\"", appPath.wstring())
        : std::format(L"\"{}\" \"{}\"", appPath.wstring(), startupCatalogPath.wstring());
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    ASSERT_TRUE(CreateProcessW(appPath.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0,
        nullptr, TestExecutableDirectory().c_str(), &startupInfo, &processInfo));
    ProcessHandle process(processInfo.hProcess);
    ProcessHandle thread(processInfo.hThread);
    AppProcessGuard appGuard(processInfo.hProcess);

    HWND mainWindow = WaitForMainWindow(processInfo.dwProcessId);
    ASSERT_NE(mainWindow, nullptr);
    ShowWindow(mainWindow, SW_RESTORE);
    SetForegroundWindow(mainWindow);
    UpdateWindow(mainWindow);

    const auto mainScreenshot = CaptureWindow(mainWindow);
    EXPECT_TRUE(SaveBmp(mainScreenshot, ArtifactPath(L"app-main-before-settings.bmp")));

    ASSERT_TRUE(PostMessageW(mainWindow, WM_COMMAND, MAKEWPARAM(ID_OPTIONS_GENERAL_SETTINGS, 0), 0));
    HWND settingsWindow = WaitForWindow(processInfo.dwProcessId, nullptr, L"Settings");
    ASSERT_NE(settingsWindow, nullptr);
    SetForegroundWindow(settingsWindow);
    UpdateWindow(settingsWindow);
    Sleep(100);
    EXPECT_TRUE(IsWindowEnabled(mainWindow));
    EXPECT_TRUE(WaitForControlText(settingsWindow, IDC_SETTINGS_HEADER_TITLE, L"General Settings"));

    const auto settingsScreenshot = CaptureWindow(settingsWindow);
    const auto settingsPath = ArtifactPath(L"settings-dialog-opened-from-app.bmp");
    EXPECT_TRUE(SaveBmp(settingsScreenshot, settingsPath));
    EXPECT_GT(settingsScreenshot.width, 500);
    EXPECT_GT(settingsScreenshot.height, 300);
    EXPECT_GT(DistinctColorBuckets(settingsScreenshot), 12)
        << "Settings screenshot looks nearly blank: " << settingsPath.string();

    RECT dialogRect{};
    ASSERT_TRUE(GetWindowRect(settingsWindow, &dialogRect));

    constexpr std::array visibleControls{
        IDC_SETTINGS_TREE,
        IDC_SETTINGS_HEADER_TITLE,
        IDC_SETTINGS_GROUP_DATE_TIME,
        IDC_SETTINGS_LABEL_DATE_TIME,
        IDC_DATE_TIME_FORMAT,
        IDC_DATE_TIME_FORMAT_SAMPLE,
        IDC_SETTINGS_GROUP_DEBUG,
        IDC_ENABLE_SCAN_FILE_DELAY,
        IDOK,
        IDCANCEL,
        IDC_SETTINGS_APPLY,
        IDHELP,
    };

    for (const auto controlId : visibleControls) {
        const HWND control = Control(settingsWindow, controlId);
        ASSERT_NE(control, nullptr) << "Missing control id " << controlId;
        EXPECT_TRUE(IsWindowVisible(control)) << "Expected visible control id " << controlId;
        EXPECT_TRUE(RectInside(dialogRect, ChildRect(settingsWindow, controlId)))
            << "Control id " << controlId << " is outside the Settings window; screenshot: "
            << settingsPath.string();
    }

    const RECT treeRect = ChildRect(settingsWindow, IDC_SETTINGS_TREE);
    for (const auto controlId : visibleControls) {
        if (controlId == IDC_SETTINGS_TREE) continue;
        EXPECT_FALSE(RectsOverlap(treeRect, ChildRect(settingsWindow, controlId)))
            << "Navigation tree overlaps control id " << controlId << "; screenshot: "
            << settingsPath.string();
    }

    constexpr std::array textMustFitControls{
        IDC_SETTINGS_HEADER_TITLE,
        IDC_SETTINGS_LABEL_DATE_TIME,
        IDC_SETTINGS_GROUP_DATE_TIME,
        IDC_SETTINGS_GROUP_DEBUG,
        IDC_ENABLE_SCAN_FILE_DELAY,
    };

    for (const auto controlId : textMustFitControls) {
        const HWND control = Control(settingsWindow, controlId);
        const auto text = WindowText(control);
        EXPECT_TRUE(TextFitsControl(control))
            << "Control id " << controlId << " clips text '" << NarrowForTest(text)
            << "'; screenshot: " << settingsPath.string();
    }

    ASSERT_TRUE(PostMessageW(mainWindow, WM_COMMAND, MAKEWPARAM(ID_OPTIONS_USER_INTERFACE_SETUP, 0), 0));
    EXPECT_TRUE(IsWindowEnabled(mainWindow));
    EXPECT_TRUE(WaitForControlText(settingsWindow, IDC_SETTINGS_HEADER_TITLE, L"User Interface Setup"));
    UpdateWindow(settingsWindow);
    Sleep(100);

    const auto userInterfaceScreenshot = CaptureWindow(settingsWindow);
    const auto userInterfacePath = ArtifactPath(L"settings-dialog-user-interface-page.bmp");
    EXPECT_TRUE(SaveBmp(userInterfaceScreenshot, userInterfacePath));
    EXPECT_GT(DistinctColorBuckets(userInterfaceScreenshot), 12)
        << "User Interface page screenshot looks nearly blank: " << userInterfacePath.string();

    constexpr std::array userInterfaceControls{
        IDC_SETTINGS_GROUP_DISPLAY,
        IDC_SHOW_STATUS_BAR,
        IDC_SHOW_TOOLBAR,
        IDC_SETTINGS_GROUP_LAYOUT,
        IDC_SETTINGS_LABEL_SPLITTER,
        IDC_MAIN_SPLITTER_POSITION,
    };

    for (const auto controlId : userInterfaceControls) {
        const HWND control = Control(settingsWindow, controlId);
        ASSERT_NE(control, nullptr) << "Missing control id " << controlId;
        EXPECT_TRUE(IsWindowVisible(control)) << "Expected visible control id " << controlId;
        EXPECT_TRUE(RectInside(dialogRect, ChildRect(settingsWindow, controlId)))
            << "Control id " << controlId << " is outside the Settings window; screenshot: "
            << userInterfacePath.string();
    }

    for (const auto controlId : {IDC_SHOW_STATUS_BAR, IDC_SHOW_TOOLBAR, IDC_SETTINGS_LABEL_SPLITTER}) {
        const HWND control = Control(settingsWindow, controlId);
        const auto text = WindowText(control);
        EXPECT_TRUE(TextFitsControl(control))
            << "Control id " << controlId << " clips text '" << NarrowForTest(text)
            << "'; screenshot: " << userInterfacePath.string();
    }

    PostMessageW(settingsWindow, WM_CLOSE, 0, 0);
    PostMessageW(mainWindow, WM_CLOSE, 0, 0);
    EXPECT_EQ(WaitForSingleObject(processInfo.hProcess, 10000), WAIT_OBJECT_0);
}
