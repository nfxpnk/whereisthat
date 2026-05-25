#include "App.h"
#include "WtlSupport.h"
#include <Windows.h>

WTL::CAppModule _Module;

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
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
