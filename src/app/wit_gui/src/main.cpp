#include "wit_gui/App.h"
#include "wit_win32/BaseWindow.h"
#include <wit_infra/Logging.h>
#include <Windows.h>
#include <format>

WTL::CAppModule _Module;

namespace {

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

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
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

    App app;
    int rc = app.Run();

    _Module.Term();
    CoUninitialize();
    WIT_LOG_INFO(std::format(L"WhereIsThat exiting with code {}", rc));
    wit::infra::ShutdownLogging();
    return rc;
}

