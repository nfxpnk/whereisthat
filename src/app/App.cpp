#include "App.h"
#include "MainFrame.h"
CAppModule _Module;
int App::Run(){ _Module.Init(nullptr, GetModuleHandle(nullptr)); MainFrame wnd; wnd.CreateEx(); wnd.ShowWindow(SW_SHOW); MSG msg; while(GetMessage(&msg,nullptr,0,0)){ TranslateMessage(&msg); DispatchMessage(&msg);} _Module.Term(); return (int)msg.wParam; }
