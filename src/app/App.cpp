#include "App.h"
#include "MainFrame.h"
#include "WtlSupport.h"
#include <Windows.h>
#include <CommCtrl.h>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

int App::Run() {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    WTL::CMessageLoop loop;
    if (!_Module.AddMessageLoop(&loop)) return 1;

    MainFrame wnd;
    if (!wnd.Create()) {
        _Module.RemoveMessageLoop();
        return 1;
    }
    if (!loop.AddMessageFilter(&wnd)) {
        wnd.DestroyWindow();
        _Module.RemoveMessageLoop();
        return 1;
    }

    wnd.Show(SW_SHOW);
    const int result = loop.Run();
    loop.RemoveMessageFilter(&wnd);
    _Module.RemoveMessageLoop();
    return result;
}
