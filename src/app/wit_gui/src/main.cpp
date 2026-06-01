#include "wit_gui/App.h"
#include "wit_win32/BaseWindow.h"
#include <Windows.h>

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
    EnableDpiAwareness();

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) return 1;

    if (FAILED(_Module.Init(nullptr, instance))) {
        CoUninitialize();
        return 1;
    }

    App app;
    int rc = app.Run();

    _Module.Term();
    CoUninitialize();
    return rc;
}

