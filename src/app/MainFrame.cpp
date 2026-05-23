#include "MainFrame.h"
#include <cwctype>
#include <shobjidl.h>
#include <string>
#include "../platform/Win32Helpers.h"

namespace {
constexpr UINT WM_SCAN_PROGRESS = WM_APP + 1;
constexpr UINT WM_SCAN_COMPLETE = WM_APP + 2;

struct ScanProgressMessage {
    std::uint64_t files{};
    std::uint64_t folders{};
};

struct NewCatalogDialogData {
    std::wstring name;
};

std::wstring NameForCatalogRoot(const std::wstring& root) {
    if (root.size() == 3 && root[1] == L':' && (root[2] == L'\\' || root[2] == L'/')) return root;
    auto end = root.find_last_not_of(L"\\/");
    if (end == std::wstring::npos) return root;
    auto pos = root.find_last_of(L"\\/", end);
    return pos == std::wstring::npos ? root.substr(0, end + 1) : root.substr(pos + 1, end - pos);
}

std::wstring TrimWhitespace(const std::wstring& value) {
    std::size_t first = 0;
    while(first < value.size() && std::iswspace(value[first])) ++first;
    std::size_t last = value.size();
    while(last > first && std::iswspace(value[last - 1])) --last;
    return value.substr(first, last - first);
}

INT_PTR CALLBACK NewCatalogDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* data = reinterpret_cast<NewCatalogDialogData*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    switch(message) {
    case WM_INITDIALOG:
        data = reinterpret_cast<NewCatalogDialogData*>(lparam);
        SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
        SendDlgItemMessageW(dialog, IDC_CATALOG_NAME, EM_SETLIMITTEXT, 255, 0);
        SetFocus(GetDlgItem(dialog, IDC_CATALOG_NAME));
        return FALSE;
    case WM_COMMAND:
        if(LOWORD(wparam) == IDOK) {
            const int length = GetWindowTextLengthW(GetDlgItem(dialog, IDC_CATALOG_NAME));
            std::wstring name(static_cast<std::size_t>(length) + 1, L'\0');
            GetDlgItemTextW(dialog, IDC_CATALOG_NAME, name.data(), length + 1);
            name.resize(static_cast<std::size_t>(length));
            data->name = TrimWhitespace(name);
            if(data->name.empty()) {
                MessageBoxW(dialog, L"Enter a catalog name.", L"New Catalog", MB_OK | MB_ICONINFORMATION);
                SetFocus(GetDlgItem(dialog, IDC_CATALOG_NAME));
                return TRUE;
            }
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        if(LOWORD(wparam) == IDCANCEL) {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
}

bool MainFrame::Create(){
    HINSTANCE instance = GetModuleHandle(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON),
        IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    if(!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if(!wc.hIconSm) wc.hIconSm = wc.hIcon;
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
    case WM_SCAN_PROGRESS: {
        auto* progress = reinterpret_cast<ScanProgressMessage*>(lparam);
        auto text = L"Scanning: " + std::to_wstring(progress->folders) + L" folders, " + std::to_wstring(progress->files) + L" files";
        SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
        delete progress;
        return 0;
    }
    case WM_SCAN_COMPLETE:
        if(worker_.joinable()) worker_.join();
        ReloadCatalogs();
        SelectCatalog(static_cast<std::int64_t>(wparam));
        SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Scan complete"));
        return 0;
    case WM_DESTROY: OnDestroy(); return 0;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

bool MainFrame::OnCreate(){
    db_.Open(dbPath_);
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
    if(!catalogs_.catalogs.empty()) SelectCatalog(catalogs_.catalogs.front().id);
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
void MainFrame::OnCommand(int id){
    if(id == ID_FILE_NEWCATALOG) OnNewCatalog();
    else if(id == ID_EDIT_ADDDISKIMAGE) OnAddOrUpdateDiskImage();
    else if(id == ID_FILE_EXIT) OnExit();
    else if(id == ID_HELP_ABOUT) OnAbout();
}
void MainFrame::ReloadCatalogs(){ catalogs_.catalogs=db_.GetCatalogs(); catalogs_.Reload(); }
void MainFrame::SelectCatalog(std::int64_t catalogId){ for(int i=0;i<ListView_GetItemCount(catalogsCtl_);++i){ LVITEMW item{}; item.mask=LVIF_PARAM; item.iItem=i; if(ListView_GetItem(catalogsCtl_, &item) && item.lParam==catalogId){ ListView_SetItemState(catalogsCtl_, i, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED); ListView_EnsureVisible(catalogsCtl_, i, FALSE); files_.SetCatalog(catalogId,&db_); break; } } }
LRESULT MainFrame::OnCatalogChanged(LPNMHDR hdr){ auto* n=(NMLISTVIEW*)hdr; if((n->uChanged&LVIF_STATE)&& (n->uNewState&LVIS_SELECTED)){ LVITEMW item{}; item.mask=LVIF_PARAM; item.iItem=n->iItem; if(ListView_GetItem(catalogsCtl_, &item)) files_.SetCatalog((std::int64_t)item.lParam,&db_);} return 0; }
LRESULT MainFrame::OnFileGetDispInfo(LPNMHDR hdr){ auto* di=(NMLVDISPINFOW*)hdr; if(di->item.mask&LVIF_TEXT){ auto t=files_.TextFor(di->item.iItem,di->item.iSubItem); lstrcpynW(di->item.pszText,t.c_str(),di->item.cchTextMax);} return 0; }
void MainFrame::OnNewCatalog(){
    if(worker_.joinable()){
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    NewCatalogDialogData data{};
    const auto result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_NEW_CATALOG),
        hwnd_, NewCatalogDialogProc, reinterpret_cast<LPARAM>(&data));
    if(result != IDOK) return;

    wit::core::Catalog catalog{};
    catalog.name = data.name;
    catalog.createdAt = wit::platform::NowIso8601();
    const auto id = db_.AddCatalog(catalog);
    ReloadCatalogs();
    SelectCatalog(id);
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Catalog created"));
}

void MainFrame::OnAddOrUpdateDiskImage(){
    if(worker_.joinable()){
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    IFileOpenDialog* dialog{};
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Choose a folder or disk to scan");
    if(SUCCEEDED(dialog->Show(hwnd_))){
        IShellItem* item{};
        if(SUCCEEDED(dialog->GetResult(&item))){
            PWSTR path{};
            if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))){
                std::wstring root(path);
                CoTaskMemFree(path);
                wit::core::Catalog catalog{};
                catalog.name = NameForCatalogRoot(root);
                catalog.rootPath = root;
                catalog.createdAt = wit::platform::NowIso8601();
                const auto id = db_.AddCatalog(catalog);
                ReloadCatalogs();
                SelectCatalog(id);
                SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Starting scan..."));
                const auto dbPath = dbPath_;
                worker_ = std::thread([this, root, id, dbPath](){
                    wit::storage::Database scanDb;
                    scanDb.Open(dbPath);
                    wit::core::FileScanner scanner;
                    scanner.ScanFolder(root, id, scanDb, [this](const wit::core::FileScanner::Progress& progress){
                        PostMessageW(hwnd_, WM_SCAN_PROGRESS, 0, reinterpret_cast<LPARAM>(
                            new ScanProgressMessage{progress.scannedFiles, progress.scannedFolders}));
                    });
                    PostMessageW(hwnd_, WM_SCAN_COMPLETE, static_cast<WPARAM>(id), 0);
                });
            }
            item->Release();
        }
    }
    dialog->Release();
}
