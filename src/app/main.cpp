#include "App.h"
#include <Windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    App app;
    int rc = app.Run();
    CoUninitialize();
    return rc;
}
