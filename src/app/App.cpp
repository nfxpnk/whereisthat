#include "App.h"
#include "MainFrame.h"
#include <Windows.h>
#include <CommCtrl.h>

int App::Run() {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    MainFrame wnd;
    if (!wnd.Create()) return 1;
    wnd.Show(SW_SHOW);
    HACCEL accelerators = LoadAcceleratorsW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDR_MAINACCEL));

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!accelerators || !TranslateAcceleratorW(wnd.WindowHandle(), accelerators, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return static_cast<int>(msg.wParam);
}
