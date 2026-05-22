#include "App.h"
#include "MainFrame.h"
#include <Windows.h>
#include <CommCtrl.h>

int App::Run(){
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    MainFrame wnd;
    if(!wnd.Create()) return 1;
    wnd.Show(SW_SHOW);

    MSG msg{};
    while(GetMessage(&msg,nullptr,0,0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
