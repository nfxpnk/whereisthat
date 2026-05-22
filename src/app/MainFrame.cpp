#include "MainFrame.h"
#include <shobjidl.h>
#include <string>
#include "../platform/Win32Helpers.h"

bool MainFrame::Create(){
    HINSTANCE instance = GetModuleHandle(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.lpfnWndProc = MainFrame::WndProc;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if(!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    HMENU menu = LoadMenuW(instance, MAKEINTRESOURCEW(IDR_MAINMENU));
    hwnd_ = CreateWindowExW(0, kClassName, L"Where Is That?", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 720, nullptr, menu, instance, this);
    return hwnd_ != nullptr;
}

void MainFrame::Show(int command){ ShowWindow(hwnd_, command); UpdateWindow(hwnd_); }

LRESULT CALLBACK MainFrame::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam){
    MainFrame* self = nullptr;
    if(message == WM_NCCREATE){
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<MainFrame*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainFrame*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT MainFrame::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam){
    switch(message){
    case WM_CREATE: return OnCreate() ? 0 : -1;
    case WM_SIZE: OnSize(LOWORD(lparam), HIWORD(lparam)); return 0;
    case WM_COMMAND: OnCommand(LOWORD(wparam)); return 0;
    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<LPNMHDR>(lparam);
        if(hdr->idFrom == IDC_CATALOGS && hdr->code == LVN_ITEMCHANGED) return OnCatalogChanged(hdr);
        if(hdr->idFrom == IDC_FILES && hdr->code == LVN_GETDISPINFOW) return OnFileGetDispInfo(hdr);
        break;
    }
    case WM_APP + 1: ReloadCatalogs(); SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Ready")); return 0;
    case WM_DESTROY: OnDestroy(); return 0;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

bool MainFrame::OnCreate(){
    db_.Open(L"catalog.db");
    status_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd_, nullptr, GetModuleHandle(nullptr), nullptr);
    catalogsCtl_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 0, 0, 0, 0,
        hwnd_, reinterpret_cast<HMENU>(IDC_CATALOGS), GetModuleHandle(nullptr), nullptr);
    filesCtl_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL, 0, 0, 0, 0,
        hwnd_, reinterpret_cast<HMENU>(IDC_FILES), GetModuleHandle(nullptr), nullptr);
    if(!status_ || !catalogsCtl_ || !filesCtl_) return false;

    catalogs_.Attach(catalogsCtl_);
    files_.Attach(filesCtl_);

    LVCOLUMNW col{LVCF_TEXT | LVCF_WIDTH | LVCF_FMT};
    col.fmt = LVCFMT_LEFT; col.cx = 220; col.pszText = const_cast<LPWSTR>(L"Catalog");
    ListView_InsertColumn(catalogsCtl_, 0, &col);
    col.cx = 200; col.pszText = const_cast<LPWSTR>(L"Name"); ListView_InsertColumn(filesCtl_, 0, &col);
    col.cx = 80; col.pszText = const_cast<LPWSTR>(L"Type"); ListView_InsertColumn(filesCtl_, 1, &col);
    col.fmt = LVCFMT_RIGHT; col.cx = 100; col.pszText = const_cast<LPWSTR>(L"Size"); ListView_InsertColumn(filesCtl_, 2, &col);
    col.fmt = LVCFMT_LEFT; col.cx = 320; col.pszText = const_cast<LPWSTR>(L"Path"); ListView_InsertColumn(filesCtl_, 3, &col);
    col.cx = 180; col.pszText = const_cast<LPWSTR>(L"Modified"); ListView_InsertColumn(filesCtl_, 4, &col);

    ReloadCatalogs();
    return true;
}

void MainFrame::OnSize(int width, int height){
    SendMessageW(status_, WM_SIZE, 0, 0);
    RECT sr{};
    GetWindowRect(status_, &sr);
    int statusHeight = sr.bottom - sr.top;
    int contentHeight = height - statusHeight;
    int left = width / 3;
    MoveWindow(catalogsCtl_, 0, 0, left, contentHeight, TRUE);
    MoveWindow(filesCtl_, left + 4, 0, width - left - 4, contentHeight, TRUE);
}

void MainFrame::OnDestroy(){ if(worker_.joinable()) worker_.join(); PostQuitMessage(0);}
void MainFrame::OnExit(){ PostMessageW(hwnd_, WM_CLOSE, 0, 0);}
void MainFrame::OnAbout(){ MessageBoxW(hwnd_, L"Where Is That?\nNative Win32 build.", L"About", MB_OK);}
void MainFrame::OnRefresh(){ ReloadCatalogs(); }
void MainFrame::OnCommand(int id){ if(id==ID_FILE_NEWCATALOG) OnNewCatalog(); else if(id==ID_FILE_REFRESH) OnRefresh(); else if(id==ID_FILE_EXIT) OnExit(); else if(id==ID_HELP_ABOUT) OnAbout(); }
void MainFrame::ReloadCatalogs(){ catalogs_.catalogs=db_.GetCatalogs(); catalogs_.Reload(); }
LRESULT MainFrame::OnCatalogChanged(LPNMHDR hdr){ auto* n=(NMLISTVIEW*)hdr; if((n->uChanged&LVIF_STATE)&& (n->uNewState&LVIS_SELECTED)){ LVITEMW item{}; item.mask=LVIF_PARAM; item.iItem=n->iItem; if(ListView_GetItem(catalogsCtl_, &item)) files_.SetCatalog((std::int64_t)item.lParam,&db_);} return 0; }
LRESULT MainFrame::OnFileGetDispInfo(LPNMHDR hdr){ auto* di=(NMLVDISPINFOW*)hdr; if(di->item.mask&LVIF_TEXT){ auto t=files_.TextFor(di->item.iItem,di->item.iSubItem); lstrcpynW(di->item.pszText,t.c_str(),di->item.cchTextMax);} return 0; }
void MainFrame::OnNewCatalog(){ IFileOpenDialog* dlg{}; if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dlg)))) return; DWORD opt{}; dlg->GetOptions(&opt); dlg->SetOptions(opt|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM); if(SUCCEEDED(dlg->Show(hwnd_))){ IShellItem* item{}; if(SUCCEEDED(dlg->GetResult(&item))){ PWSTR path{}; if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){ std::wstring root(path); CoTaskMemFree(path); wit::core::Catalog c{}; c.name=root.substr(root.find_last_of(L"\\/")+1); c.rootPath=root; c.createdAt=wit::platform::NowIso8601(); auto id=db_.AddCatalog(c); if(worker_.joinable()) worker_.join(); worker_=std::thread([this,root,id](){ wit::core::FileScanner s; s.ScanFolder(root,id,db_,[this](const wit::core::FileScanner::Progress& p){ auto text=L"Scanning: "+std::to_wstring(p.scannedFiles); SendMessageW(status_,SB_SETTEXTW,0,reinterpret_cast<LPARAM>(text.c_str())); }); PostMessageW(hwnd_,WM_APP+1,0,0);}); ReloadCatalogs(); } item->Release(); }} dlg->Release(); }
