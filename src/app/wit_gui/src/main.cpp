#include "wit_gui/App.h"
#include "wit_win32/BaseWindow.h"
#include <wit_infra/AppSettings.h>
#include <wit_infra/Logging.h>
#include <Windows.h>
#include <Shellapi.h>
#include <cwchar>
#include <format>
#include <string>

WTL::CAppModule _Module;

namespace {

constexpr wchar_t kBypassAlphaWarningArgument[] = L"--bypass-alpha-warning";

struct StartupOptions {
    std::wstring catalogPath;
    bool bypassAlphaWarning{};
};

bool IsBypassAlphaWarningArgument(const wchar_t* argument) {
    return argument && ::_wcsicmp(argument, kBypassAlphaWarningArgument) == 0;
}

StartupOptions ParseCommandLine() {
    int argc{};
    const auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return {};

    StartupOptions options;
    for (int index = 1; index < argc; ++index) {
        if (IsBypassAlphaWarningArgument(argv[index])) {
            options.bypassAlphaWarning = true;
        } else if (options.catalogPath.empty()) {
            options.catalogPath = argv[index];
        }
    }

    LocalFree(argv);
    return options;
}

void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;

    using SetProcessDpiAwarenessContextProc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setProcessDpiAwarenessContext =
        reinterpret_cast<SetProcessDpiAwarenessContextProc>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setProcessDpiAwarenessContext) {
        if (setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
        if (setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) return;
    }

    using SetProcessDpiAwareProc = BOOL(WINAPI*)();
    auto setProcessDpiAware =
        reinterpret_cast<SetProcessDpiAwareProc>(GetProcAddress(user32, "SetProcessDPIAware"));
    if (setProcessDpiAware) {
        setProcessDpiAware();
    }
}

void ShowAlphaWarning() {
    MessageBoxW(nullptr,
        L"Where Is That? is currently in alpha stage.\n\n"
        L"It may contain bugs, crash, lose unsaved changes, or produce incomplete or incorrect catalog data. "
        L"Please keep backups of important catalogs and source data.\n\n"
        L"Use this application at your own risk.",
        L"Where Is That? Alpha Warning",
        MB_OK | MB_ICONWARNING);
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const auto startupOptions = ParseCommandLine();
    wit::infra::InitializeLogging();
    WIT_LOG_INFO(L"WhereIsThat starting");
    EnableDpiAwareness();

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) {
        WIT_LOG_ERROR(std::format(L"CoInitializeEx failed: 0x{:08X}", static_cast<unsigned int>(comResult)));
        wit::infra::ShutdownLogging();
        return 1;
    }

    if (FAILED(_Module.Init(nullptr, instance))) {
        WIT_LOG_ERROR(L"WTL module initialization failed");
        CoUninitialize();
        wit::infra::ShutdownLogging();
        return 1;
    }

    const auto settings = wit::platform::LoadAppSettings();
    if (!startupOptions.bypassAlphaWarning && !settings.doNotShowAlphaWarning) {
        ShowAlphaWarning();
    }

    App app(startupOptions.catalogPath);
    int rc = app.Run();

    _Module.Term();
    CoUninitialize();
    WIT_LOG_INFO(std::format(L"WhereIsThat exiting with code {}", rc));
    wit::infra::ShutdownLogging();
    return rc;
}

