#include "wit_gui/App.h"
#include "wit_gui/MainWindow.h"
#include "wit_win32/BaseWindow.h"
#include <wit_infra/Logging.h>
#include <Windows.h>
#include <CommCtrl.h>
#include <format>
#include <utility>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

App::App(std::wstring startupCatalogPath) : startupCatalogPath_(std::move(startupCatalogPath)) {}

int App::Run() {
    WIT_LOG_INFO(L"app message loop initialization");
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&icc)) {
        WIT_LOG_WARN(L"InitCommonControlsEx failed");
    }

    WTL::CMessageLoop loop;
    if (!_Module.AddMessageLoop(&loop)) {
        WIT_LOG_ERROR(L"AddMessageLoop failed");
        return 1;
    }

    MainFrame wnd(std::move(startupCatalogPath_));
    if (!wnd.Create()) {
        WIT_LOG_ERROR(L"main window creation failed");
        _Module.RemoveMessageLoop();
        return 1;
    }
    if (!loop.AddMessageFilter(&wnd)) {
        WIT_LOG_ERROR(L"AddMessageFilter failed");
        wnd.DestroyWindow();
        _Module.RemoveMessageLoop();
        return 1;
    }

    wnd.Show(SW_SHOW);
    WIT_LOG_INFO(L"entering app message loop");
    const int result = loop.Run();
    WIT_LOG_INFO(std::format(L"message loop exited with code {}", result));
    loop.RemoveMessageFilter(&wnd);
    _Module.RemoveMessageLoop();
    return result;
}

