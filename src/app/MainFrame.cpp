#include "MainFrame.h"
#include <algorithm>
#include <shobjidl.h>
#include <string>
#include <utility>
#include <vector>
#include "../platform/Win32Helpers.h"

namespace {
constexpr UINT WM_SCAN_PROGRESS = WM_APP + 1;
constexpr UINT WM_SCAN_COMPLETE = WM_APP + 2;

struct ScanProgressMessage {
    std::uint64_t files{};
    std::uint64_t folders{};
};

struct GeneralSettingsDialogData {
    wit::platform::AppSettings settings;
};

constexpr COMDLG_FILTERSPEC kCatalogFileTypes[] = {
    {L"SQLite catalog database (*.db)", L"*.db"},
    {L"All files (*.*)", L"*.*"}
};

std::wstring NameForCatalogRoot(const std::wstring& root) {
    if (root.size() == 3 && root[1] == L':' && (root[2] == L'\\' || root[2] == L'/')) return root;
    auto end = root.find_last_not_of(L"\\/");
    if (end == std::wstring::npos) return root;
    auto pos = root.find_last_of(L"\\/", end);
    return pos == std::wstring::npos ? root.substr(0, end + 1) : root.substr(pos + 1, end - pos);
}

std::wstring NormalizedSourcePath(const std::wstring& path) {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
        if (length == 0) return path;
        if (length < buffer.size()) {
            std::wstring normalized(buffer.data(), length);
            while (normalized.size() > 3 && (normalized.back() == L'\\' || normalized.back() == L'/')) normalized.pop_back();
            return normalized;
        }
        buffer.resize(static_cast<std::size_t>(length) + 1);
    }
}

bool ItemFileSystemPath(IShellItem* item, std::wstring& path) {
    PWSTR selected{};
    if(FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &selected))) return false;
    path = selected;
    CoTaskMemFree(selected);
    return true;
}

bool ChooseNewCatalogPath(HWND owner, std::wstring& path) {
    IFileSaveDialog* dialog{};
    if(FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return false;
    dialog->SetTitle(L"Create a new catalog database");
    dialog->SetFileTypes(2, kCatalogFileTypes);
    dialog->SetFileTypeIndex(1);
    dialog->SetDefaultExtension(L"db");
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    bool selected = false;
    if(SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item{};
        if(SUCCEEDED(dialog->GetResult(&item))) {
            selected = ItemFileSystemPath(item, path);
            item->Release();
        }
    }
    dialog->Release();
    return selected;
}

bool ChooseCatalogToOpen(HWND owner, std::wstring& path) {
    IFileOpenDialog* dialog{};
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return false;
    dialog->SetTitle(L"Open a catalog database");
    dialog->SetFileTypes(2, kCatalogFileTypes);
    dialog->SetFileTypeIndex(1);
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
    bool selected = false;
    if(SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item{};
        if(SUCCEEDED(dialog->GetResult(&item))) {
            selected = ItemFileSystemPath(item, path);
            item->Release();
        }
    }
    dialog->Release();
    return selected;
}

INT_PTR CALLBACK GeneralSettingsDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* data = reinterpret_cast<GeneralSettingsDialogData*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    switch(message) {
    case WM_INITDIALOG:
        data = reinterpret_cast<GeneralSettingsDialogData*>(lparam);
        SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
        CheckDlgButton(dialog, IDC_SHOW_STATUS_BAR, data->settings.showStatusBar ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    case WM_COMMAND:
        if(LOWORD(wparam) == IDOK) {
            data->settings.showStatusBar = IsDlgButtonChecked(dialog, IDC_SHOW_STATUS_BAR) == BST_CHECKED;
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
    case WM_SIZE:
        if(wparam != SIZE_MINIMIZED) OnSize(LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_LBUTTONDOWN: {
        const int x = static_cast<short>(LOWORD(lparam));
        const int y = static_cast<short>(HIWORD(lparam));
        if(IsOverSplitter(x, y)) {
            splitterDragging_ = true;
            splitterDragOffset_ = x - splitterPosition_;
            SetCapture(hwnd_);
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if(splitterDragging_) {
            RECT client{};
            GetClientRect(hwnd_, &client);
            splitterPosition_ = static_cast<short>(LOWORD(lparam)) - splitterDragOffset_;
            OnSize(client.right - client.left, client.bottom - client.top);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if(splitterDragging_) {
            splitterDragging_ = false;
            if(GetCapture() == hwnd_) ReleaseCapture();
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        splitterDragging_ = false;
        break;
    case WM_SETCURSOR: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(hwnd_, &point);
        if(splitterDragging_ ||
            (LOWORD(lparam) == HTCLIENT && IsOverSplitter(point.x, point.y))) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        break;
    }
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
        if(lparam != 0) {
            ReloadCatalogs();
            SelectCatalog(static_cast<std::int64_t>(wparam));
            SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Scan complete"));
        } else {
            SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Scan failed; catalog unchanged"));
        }
        return 0;
    case WM_DESTROY: OnDestroy(); return 0;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

bool MainFrame::OnCreate(){
    settings_ = wit::platform::LoadAppSettings();
    DWORD statusStyle = WS_CHILD;
    if(settings_.showStatusBar) statusStyle |= WS_VISIBLE;
    status_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, statusStyle,
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
    col.fmt = LVCFMT_LEFT; col.cx = 220; col.pszText = const_cast<LPWSTR>(L"Media Source");
    ListView_InsertColumn(catalogsCtl_, 0, &col);
    col.cx = 200; col.pszText = const_cast<LPWSTR>(L"Name"); ListView_InsertColumn(filesCtl_, 0, &col);
    col.cx = 80; col.pszText = const_cast<LPWSTR>(L"Type"); ListView_InsertColumn(filesCtl_, 1, &col);
    col.fmt = LVCFMT_RIGHT; col.cx = 100; col.pszText = const_cast<LPWSTR>(L"Size"); ListView_InsertColumn(filesCtl_, 2, &col);
    col.fmt = LVCFMT_LEFT; col.cx = 320; col.pszText = const_cast<LPWSTR>(L"Path"); ListView_InsertColumn(filesCtl_, 3, &col);
    col.cx = 180; col.pszText = const_cast<LPWSTR>(L"Modified"); ListView_InsertColumn(filesCtl_, 4, &col);

    ClearCatalogViews();
    if(!settings_.lastCatalogPath.empty() && !ActivateCatalog(settings_.lastCatalogPath, false, false)) {
        SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Last used catalog is unavailable"));
    }
    return true;
}

void MainFrame::OnSize(int width, int height){
    int statusHeight = 0;
    if(settings_.showStatusBar) {
        SendMessageW(status_, WM_SIZE, 0, 0);
        RECT sr{};
        GetWindowRect(status_, &sr);
        statusHeight = sr.bottom - sr.top;
    }
    contentHeight_ = (std::max)(0, height - statusHeight);
    const int availableWidth = (std::max)(0, width - kSplitterWidth);
    const int minimumWidth = (std::min)(kMinPaneWidth, availableWidth / 2);
    if(splitterPosition_ < 0) splitterPosition_ = width / 3;
    splitterPosition_ = std::clamp(splitterPosition_, minimumWidth, availableWidth - minimumWidth);
    MoveWindow(catalogsCtl_, 0, 0, splitterPosition_, contentHeight_, TRUE);
    MoveWindow(filesCtl_, splitterPosition_ + kSplitterWidth, 0,
        availableWidth - splitterPosition_, contentHeight_, TRUE);
}

bool MainFrame::IsOverSplitter(int x, int y) const {
    return x >= splitterPosition_ && x < splitterPosition_ + kSplitterWidth &&
        y >= 0 && y < contentHeight_;
}

void MainFrame::OnDestroy(){ if(worker_.joinable()) worker_.join(); PostQuitMessage(0);}
void MainFrame::OnExit(){ PostMessageW(hwnd_, WM_CLOSE, 0, 0);}
void MainFrame::OnAbout(){ MessageBoxW(hwnd_, L"Where Is That?\nNative Win32 build.", L"About", MB_OK);}
void MainFrame::OnCommand(int id){
    if(id == ID_FILE_NEWCATALOG) OnNewCatalog();
    else if(id == ID_FILE_OPEN) OnOpenCatalog();
    else if(id == ID_EDIT_ADDDISKIMAGE) OnAddOrUpdateDiskImage();
    else if(id == ID_OPTIONS_GENERAL_SETTINGS) OnGeneralSettings();
    else if(id == ID_FILE_EXIT) OnExit();
    else if(id == ID_HELP_ABOUT) OnAbout();
}
void MainFrame::ClearCatalogViews(){ catalogs_.catalogs.clear(); catalogs_.Reload(); files_.SetCatalog(0, nullptr); }
void MainFrame::ReloadCatalogs(){ catalogs_.catalogs=db_.IsOpen()?db_.GetCatalogs():std::vector<wit::core::Catalog>{}; catalogs_.Reload(); if(catalogs_.catalogs.empty()) files_.SetCatalog(0, nullptr); }
void MainFrame::SelectCatalog(std::int64_t catalogId){ for(int i=0;i<ListView_GetItemCount(catalogsCtl_);++i){ LVITEMW item{}; item.mask=LVIF_PARAM; item.iItem=i; if(ListView_GetItem(catalogsCtl_, &item) && item.lParam==catalogId){ ListView_SetItemState(catalogsCtl_, i, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED); ListView_EnsureVisible(catalogsCtl_, i, FALSE); files_.SetCatalog(catalogId,&db_); break; } } }
LRESULT MainFrame::OnCatalogChanged(LPNMHDR hdr){ auto* n=(NMLISTVIEW*)hdr; if((n->uChanged&LVIF_STATE)&& (n->uNewState&LVIS_SELECTED)){ LVITEMW item{}; item.mask=LVIF_PARAM; item.iItem=n->iItem; if(ListView_GetItem(catalogsCtl_, &item)) files_.SetCatalog((std::int64_t)item.lParam,&db_);} return 0; }
LRESULT MainFrame::OnFileGetDispInfo(LPNMHDR hdr){ auto* di=(NMLVDISPINFOW*)hdr; if(di->item.mask&LVIF_TEXT){ auto t=files_.TextFor(di->item.iItem,di->item.iSubItem); lstrcpynW(di->item.pszText,t.c_str(),di->item.cchTextMax);} return 0; }
bool MainFrame::ActivateCatalog(const std::wstring& path, bool createNew, bool persistPath) {
    wit::storage::Database candidate;
    const bool opened = createNew ? candidate.CreateNew(path) : candidate.OpenExisting(path);
    if(!opened) return false;
    db_ = std::move(candidate);
    activeCatalogPath_ = path;
    ClearCatalogViews();
    ReloadCatalogs();
    if(!catalogs_.catalogs.empty()) SelectCatalog(catalogs_.catalogs.front().id);
    if(persistPath) {
        settings_.lastCatalogPath = path;
        if(!wit::platform::SaveAppSettings(settings_)) {
            MessageBoxW(hwnd_, L"The catalog opened, but its path could not be saved in settings.ini.",
                L"Catalog Settings", MB_OK | MB_ICONWARNING);
        }
    }
    return true;
}
void MainFrame::OnNewCatalog(){
    if(worker_.joinable()){
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring path;
    if(!ChooseNewCatalogPath(hwnd_, path)) return;
    if(!ActivateCatalog(path, true, true)) {
        MessageBoxW(hwnd_, L"Unable to create the new catalog. Choose a filename that does not already exist.",
            L"New Catalog", MB_OK | MB_ICONERROR);
        return;
    }
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Catalog created"));
}

void MainFrame::OnOpenCatalog(){
    if(worker_.joinable()){
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring path;
    if(!ChooseCatalogToOpen(hwnd_, path)) return;
    if(!ActivateCatalog(path, false, true)) {
        MessageBoxW(hwnd_, L"The selected file is not an available catalog database.", L"Open Catalog",
            MB_OK | MB_ICONERROR);
        return;
    }
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Catalog opened"));
}

void MainFrame::OnGeneralSettings(){
    GeneralSettingsDialogData data{settings_};
    const auto result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_GENERAL_SETTINGS),
        hwnd_, GeneralSettingsDialogProc, reinterpret_cast<LPARAM>(&data));
    if(result != IDOK) return;
    if(!wit::platform::SaveAppSettings(data.settings)) {
        MessageBoxW(hwnd_, L"Unable to save settings.ini.", L"General Settings", MB_OK | MB_ICONERROR);
        return;
    }
    settings_ = data.settings;
    ApplyDisplaySettings();
}

void MainFrame::ApplyDisplaySettings(){
    ShowWindow(status_, settings_.showStatusBar ? SW_SHOW : SW_HIDE);
    RECT client{};
    GetClientRect(hwnd_, &client);
    OnSize(client.right - client.left, client.bottom - client.top);
}

void MainFrame::OnAddOrUpdateDiskImage(){
    if(worker_.joinable()){
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if(!db_.IsOpen() || activeCatalogPath_.empty()){
        MessageBoxW(hwnd_, L"Create or open a catalog before adding a disk image.", L"No Active Catalog",
            MB_OK | MB_ICONINFORMATION);
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
                std::wstring root = NormalizedSourcePath(path);
                CoTaskMemFree(path);
                SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Starting scan..."));
                const auto dbPath = activeCatalogPath_;
                worker_ = std::thread([this, root, dbPath](){
                    wit::storage::Database scanDb;
                    std::int64_t id{};
                    bool success = scanDb.OpenExisting(dbPath) && scanDb.BeginTransaction();
                    if(success) {
                        id = scanDb.FindCatalogByRootPath(root);
                        if(id != 0) {
                            success = scanDb.DeleteDuplicateCatalogsForRootPath(root, id) &&
                                scanDb.DeleteFilesForCatalog(id);
                        } else {
                            wit::core::Catalog catalog{};
                            catalog.name = NameForCatalogRoot(root);
                            catalog.rootPath = root;
                            catalog.createdAt = wit::platform::NowIso8601();
                            id = scanDb.AddCatalog(catalog);
                            success = id != 0;
                        }
                    }
                    if(success) {
                        wit::core::FileScanner scanner;
                        success = scanner.ScanFolder(root, id, scanDb, [this](const wit::core::FileScanner::Progress& progress){
                            PostMessageW(hwnd_, WM_SCAN_PROGRESS, 0, reinterpret_cast<LPARAM>(
                                new ScanProgressMessage{progress.scannedFiles, progress.scannedFolders}));
                        }, false);
                    }
                    if(success) {
                        if(!scanDb.Commit()) {
                            scanDb.Rollback();
                            success = false;
                        }
                    } else if(scanDb.IsOpen()) {
                        scanDb.Rollback();
                    }
                    PostMessageW(hwnd_, WM_SCAN_COMPLETE, static_cast<WPARAM>(id), success ? 1 : 0);
                });
            }
            item->Release();
        }
    }
    dialog->Release();
}
