#include "MainFrame.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <shobjidl.h>
#include <string>
#include <utility>
#include <vector>
#include <wincodec.h>
#include "../platform/Win32Helpers.h"
#include "../ui/SearchDialog.h"

namespace {
constexpr UINT WM_SCAN_PROGRESS = WM_APP + 1;
constexpr UINT WM_SCAN_COMPLETE = WM_APP + 2;
constexpr int kToolbarIconSize = 16;
constexpr int kToolbarButtonSize = 25;
constexpr int kNavigationHeight = 32;

std::wstring JoinStoredPath(const std::wstring& parent, const std::wstring& name) {
    if (parent.empty()) return name;
    const auto last = parent.back();
    return last == L'\\' || last == L'/' ? parent + name : parent + L"\\" + name;
}

template<typename T>
void ReleaseIfPresent(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

HBITMAP LoadToolbarBitmap(IWICImagingFactory* factory, UINT resourceId) {
    const auto instance = GetModuleHandleW(nullptr);
    const auto resource = FindResourceW(instance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) return nullptr;
    const auto resourceData = LoadResource(instance, resource);
    const auto size = SizeofResource(instance, resource);
    const auto bytes = resourceData ? LockResource(resourceData) : nullptr;
    if (!bytes || size == 0) return nullptr;

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return nullptr;
    auto* destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(destination, bytes, size);
    GlobalUnlock(memory);

    IStream* stream{};
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return nullptr;
    }
    IWICBitmapDecoder* decoder{};
    IWICBitmapFrameDecode* frame{};
    IWICFormatConverter* converter{};
    HBITMAP bitmap{};
    void* pixels{};
    if (SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame))) {
        UINT width{};
        UINT height{};
        if (SUCCEEDED(frame->GetSize(&width, &height)) && width == kToolbarIconSize && height == kToolbarIconSize &&
            SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
            SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(info.bmiHeader);
            info.bmiHeader.biWidth = kToolbarIconSize;
            info.bmiHeader.biHeight = -kToolbarIconSize;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            if (!bitmap || FAILED(converter->CopyPixels(nullptr, kToolbarIconSize * 4,
                kToolbarIconSize * kToolbarIconSize * 4, static_cast<BYTE*>(pixels)))) {
                if (bitmap) DeleteObject(bitmap);
                bitmap = nullptr;
            }
        }
    }
    ReleaseIfPresent(converter);
    ReleaseIfPresent(frame);
    ReleaseIfPresent(decoder);
    ReleaseIfPresent(stream);
    return bitmap;
}

const wchar_t* ToolbarTooltipText(int commandId) {
    switch (commandId) {
    case ID_FILE_NEWCATALOG:
    case ID_FILE_OPEN:
        return L"Catalog";
    case ID_FILE_SAVE:
        return L"Save current catalog";
    case ID_FILE_REPORT_GENERATOR:
        return L"Generate Report";
    case ID_EDIT_ADDDISKIMAGE:
        return L"Add/Update Disk Image in Catalog";
    case ID_SEARCH_FOR_ITEMS:
        return L"Search";
    case ID_ACTIONS_EDIT_DESCRIPTION:
        return L"Edit Description";
    case ID_ACTIONS_PROPERTIES:
        return L"Item Properties";
    case ID_ACTIONS_OPEN_EXPLORER:
        return L"Open In Explorer";
    case ID_ACTIONS_VIEW_FILE:
        return L"View File";
    case ID_VIEW_DETAILS:
        return L"View Details";
    case ID_TOOLBAR_SORT_NAME:
        return L"Sort By Name";
    case ID_TOOLBAR_SORT_EXTENSION:
        return L"Sort By Extension";
    case ID_TOOLBAR_SORT_SIZE:
        return L"Sort By Size";
    case ID_TOOLBAR_SORT_DATE:
        return L"Sort By Date";
    case ID_TOOLBAR_SORT_REVERSE:
        return L"Reverse Sort Order";
    default:
        return L"";
    }
}

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
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &selected))) return false;
    path = selected;
    CoTaskMemFree(selected);
    return true;
}

bool ChooseNewCatalogPath(HWND owner, std::wstring& path) {
    IFileSaveDialog* dialog{};
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return false;
    dialog->SetTitle(L"Create a new catalog database");
    dialog->SetFileTypes(2, kCatalogFileTypes);
    dialog->SetFileTypeIndex(1);
    dialog->SetDefaultExtension(L"db");
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    bool selected = false;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item{};
        if (SUCCEEDED(dialog->GetResult(&item))) {
            selected = ItemFileSystemPath(item, path);
            item->Release();
        }
    }
    dialog->Release();
    return selected;
}

bool ChooseCatalogToOpen(HWND owner, std::wstring& path) {
    IFileOpenDialog* dialog{};
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return false;
    dialog->SetTitle(L"Open a catalog database");
    dialog->SetFileTypes(2, kCatalogFileTypes);
    dialog->SetFileTypeIndex(1);
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
    bool selected = false;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item{};
        if (SUCCEEDED(dialog->GetResult(&item))) {
            selected = ItemFileSystemPath(item, path);
            item->Release();
        }
    }
    dialog->Release();
    return selected;
}

INT_PTR CALLBACK GeneralSettingsDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* data = reinterpret_cast<GeneralSettingsDialogData*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    switch (message) {
    case WM_INITDIALOG:
        data = reinterpret_cast<GeneralSettingsDialogData*>(lparam);
        SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
        CheckDlgButton(dialog, IDC_SHOW_STATUS_BAR, data->settings.showStatusBar ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextW(dialog, IDC_LAST_OPENED_CATALOG, data->settings.lastCatalogPath.empty()
            ? L"(No catalog opened)" : data->settings.lastCatalogPath.c_str());
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDOK) {
            data->settings.showStatusBar = IsDlgButtonChecked(dialog, IDC_SHOW_STATUS_BAR) == BST_CHECKED;
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        if (LOWORD(wparam) == IDCANCEL) {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
}

bool MainFrame::Create() {
    HINSTANCE instance = GetModuleHandle(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON),
        IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if (!wc.hIconSm) wc.hIconSm = wc.hIcon;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.lpfnWndProc = MainFrame::WndProc;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    HMENU menu = LoadMenuW(instance, MAKEINTRESOURCEW(IDR_MAINMENU));
    hwnd_ = CreateWindowExW(0, kClassName, L"Where Is That?", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 720, nullptr, menu, instance, this);
    return hwnd_ != nullptr;
}

void MainFrame::Show(int command) {
    ShowWindow(hwnd_, command);
    UpdateWindow(hwnd_);
}

LRESULT CALLBACK MainFrame::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    MainFrame* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<MainFrame*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainFrame*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT MainFrame::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        return OnCreate() ? 0 : -1;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) OnSize(LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_LBUTTONDOWN: {
        const int x = static_cast<short>(LOWORD(lparam));
        const int y = static_cast<short>(HIWORD(lparam));
        if (IsOverSplitter(x, y)) {
            splitterDragging_ = true;
            splitterDragOffset_ = x - splitterPosition_;
            SetCapture(hwnd_);
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if (splitterDragging_) {
            RECT client{};
            GetClientRect(hwnd_, &client);
            splitterPosition_ = static_cast<short>(LOWORD(lparam)) - splitterDragOffset_;
            OnSize(client.right - client.left, client.bottom - client.top);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (splitterDragging_) {
            splitterDragging_ = false;
            if (GetCapture() == hwnd_) ReleaseCapture();
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
        if (splitterDragging_ ||
            (LOWORD(lparam) == HTCLIENT && IsOverSplitter(point.x, point.y))) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        OnCommand(LOWORD(wparam));
        return 0;
    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<LPNMHDR>(lparam);
        if (hdr->idFrom == IDC_BROWSER_TREE && hdr->code == TVN_SELCHANGEDW) return OnTreeSelectionChanged(hdr);
        if (hdr->idFrom == IDC_BROWSER_TREE && hdr->code == TVN_ITEMEXPANDINGW) return OnTreeExpanding(hdr);
        if (hdr->idFrom == IDC_FILES && hdr->code == LVN_GETDISPINFOW) return OnFileGetDispInfo(hdr);
        if (hdr->idFrom == IDC_FILES && hdr->code == LVN_ITEMACTIVATE) return OnFileActivate(hdr);
        if (hdr->idFrom == IDC_TOOLBAR && hdr->code == TBN_DROPDOWN) {
            return OnToolbarDropDown(reinterpret_cast<LPNMTOOLBAR>(hdr));
        }
        if (hdr->idFrom == IDC_TOOLBAR && hdr->code == TBN_GETINFOTIPW) {
            auto* tip = reinterpret_cast<LPNMTBGETINFOTIPW>(hdr);
            lstrcpynW(tip->pszText, ToolbarTooltipText(tip->iItem), tip->cchTextMax);
            return 0;
        }
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
        if (worker_.joinable()) worker_.join();
        if (lparam != 0) {
            ReloadBrowser();
            SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Scan complete"));
        } else {
            SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Scan failed; catalog unchanged"));
        }
        return 0;
    case WM_DESTROY:
        OnDestroy();
        return 0;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

bool MainFrame::OnCreate() {
    settings_ = wit::platform::LoadAppSettings();
    RefreshOpenRecentMenu();
    if (!CreateToolbar()) return false;
    DWORD statusStyle = WS_CHILD;
    if (settings_.showStatusBar) statusStyle |= WS_VISIBLE;
    status_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, statusStyle,
        0, 0, 0, 0, hwnd_, nullptr, GetModuleHandle(nullptr), nullptr);
    treeCtl_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
        TVS_SHOWSELALWAYS, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BROWSER_TREE),
        GetModuleHandle(nullptr), nullptr);
    backCtl_ = CreateWindowExW(0, L"BUTTON", L"<", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BROWSER_BACK), GetModuleHandle(nullptr), nullptr);
    forwardCtl_ = CreateWindowExW(0, L"BUTTON", L">", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BROWSER_FORWARD), GetModuleHandle(nullptr), nullptr);
    addressCtl_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY, 0, 0, 0, 0,
        hwnd_, reinterpret_cast<HMENU>(IDC_BROWSER_ADDRESS), GetModuleHandle(nullptr), nullptr);
    filesCtl_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL, 0, 0, 0, 0,
        hwnd_, reinterpret_cast<HMENU>(IDC_FILES), GetModuleHandle(nullptr), nullptr);
    if (!status_ || !treeCtl_ || !backCtl_ || !forwardCtl_ || !addressCtl_ || !filesCtl_) return false;

    tree_.Attach(treeCtl_);
    files_.Attach(filesCtl_);

    LVCOLUMNW col{LVCF_TEXT | LVCF_WIDTH | LVCF_FMT};
    col.fmt = LVCFMT_LEFT;
    col.cx = 200;
    col.pszText = const_cast<LPWSTR>(L"Name");
    ListView_InsertColumn(filesCtl_, 0, &col);
    col.cx = 80;
    col.pszText = const_cast<LPWSTR>(L"Type");
    ListView_InsertColumn(filesCtl_, 1, &col);
    col.fmt = LVCFMT_RIGHT;
    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(L"Size");
    ListView_InsertColumn(filesCtl_, 2, &col);
    col.fmt = LVCFMT_LEFT;
    col.cx = 320;
    col.pszText = const_cast<LPWSTR>(L"Path");
    ListView_InsertColumn(filesCtl_, 3, &col);
    col.cx = 180;
    col.pszText = const_cast<LPWSTR>(L"Modified");
    ListView_InsertColumn(filesCtl_, 4, &col);

    ClearCatalogViews();
    if (!settings_.lastCatalogPath.empty() && !ActivateCatalog(settings_.lastCatalogPath, false, false)) {
        SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Last used catalog is unavailable"));
    }
    return true;
}

bool MainFrame::CreateToolbar() {
    toolbar_ = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_TOP | CCS_NODIVIDER,
        0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_TOOLBAR), GetModuleHandleW(nullptr), nullptr);
    if (!toolbar_) return false;

    SendMessageW(toolbar_, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(toolbar_, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
    SendMessageW(toolbar_, TB_SETBITMAPSIZE, 0, MAKELONG(kToolbarIconSize, kToolbarIconSize));
    SendMessageW(toolbar_, TB_SETBUTTONSIZE, 0, MAKELONG(kToolbarButtonSize, kToolbarButtonSize));

    toolbarImages_ = ImageList_Create(kToolbarIconSize, kToolbarIconSize, ILC_COLOR32, 16, 0);
    if (!toolbarImages_) return false;

    IWICImagingFactory* factory{};
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)))) return false;
    constexpr std::array<UINT, 16> imageIds = {
        IDB_TOOLBAR_NEW_CATALOG, IDB_TOOLBAR_OPEN, IDB_TOOLBAR_SAVE, IDB_TOOLBAR_REPORT,
        IDB_TOOLBAR_ADD_DISK_IMAGE, IDB_TOOLBAR_SEARCH, IDB_TOOLBAR_EDIT_DESCRIPTION, IDB_TOOLBAR_PROPERTIES,
        IDB_TOOLBAR_OPEN_EXPLORER, IDB_TOOLBAR_VIEW_FILE, IDB_TOOLBAR_VIEW_DETAILS,
        IDB_TOOLBAR_SORT_NAME, IDB_TOOLBAR_SORT_EXTENSION, IDB_TOOLBAR_SORT_SIZE,
        IDB_TOOLBAR_SORT_DATE, IDB_TOOLBAR_SORT_REVERSE
    };
    for (const auto id : imageIds) {
        const auto bitmap = LoadToolbarBitmap(factory, id);
        if (!bitmap || ImageList_Add(toolbarImages_, bitmap, nullptr) == -1) {
            if (bitmap) DeleteObject(bitmap);
            factory->Release();
            return false;
        }
        DeleteObject(bitmap);
    }
    factory->Release();
    SendMessageW(toolbar_, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(toolbarImages_));

    std::vector<TBBUTTON> buttons;
    const auto addButton = [&buttons](int image, int command, BYTE style) {
        TBBUTTON button{};
        button.iBitmap = image;
        button.idCommand = command;
        button.fsState = TBSTATE_ENABLED;
        button.fsStyle = style;
        button.iString = -1;
        buttons.push_back(button);
    };
    const auto addSeparator = [&buttons]() {
        TBBUTTON separator{};
        separator.fsStyle = BTNS_SEP;
        separator.iBitmap = 8;
        buttons.push_back(separator);
    };

    addButton(0, ID_FILE_NEWCATALOG, BTNS_BUTTON);
    addButton(1, ID_FILE_OPEN, BTNS_BUTTON);
    addButton(2, ID_FILE_SAVE, BTNS_BUTTON);
    addButton(3, ID_FILE_REPORT_GENERATOR, BTNS_BUTTON);
    addSeparator();
    addButton(4, ID_EDIT_ADDDISKIMAGE, BTNS_BUTTON);
    addButton(5, ID_SEARCH_FOR_ITEMS, BTNS_BUTTON);
    addSeparator();
    addButton(6, ID_ACTIONS_EDIT_DESCRIPTION, BTNS_BUTTON);
    addButton(7, ID_ACTIONS_PROPERTIES, BTNS_BUTTON);
    addButton(8, ID_ACTIONS_OPEN_EXPLORER, BTNS_BUTTON);
    addButton(9, ID_ACTIONS_VIEW_FILE, BTNS_BUTTON);
    addSeparator();
    addButton(10, ID_VIEW_DETAILS, BTNS_DROPDOWN);
    addSeparator();
    addButton(11, ID_TOOLBAR_SORT_NAME, BTNS_CHECK);
    addButton(12, ID_TOOLBAR_SORT_EXTENSION, BTNS_CHECK);
    addButton(13, ID_TOOLBAR_SORT_SIZE, BTNS_CHECK);
    addButton(14, ID_TOOLBAR_SORT_DATE, BTNS_CHECK);
    addButton(15, ID_TOOLBAR_SORT_REVERSE, BTNS_CHECK);
    if (!SendMessageW(toolbar_, TB_ADDBUTTONSW, static_cast<WPARAM>(buttons.size()),
        reinterpret_cast<LPARAM>(buttons.data()))) {
        return false;
    }
    SendMessageW(toolbar_, TB_AUTOSIZE, 0, 0);
    return true;
}

void MainFrame::OnSize(int width, int height) {
    if (toolbar_) {
        SendMessageW(toolbar_, TB_AUTOSIZE, 0, 0);
        RECT toolbarRect{};
        GetWindowRect(toolbar_, &toolbarRect);
        toolbarHeight_ = toolbarRect.bottom - toolbarRect.top;
    }
    int statusHeight = 0;
    if (settings_.showStatusBar) {
        SendMessageW(status_, WM_SIZE, 0, 0);
        RECT sr{};
        GetWindowRect(status_, &sr);
        statusHeight = sr.bottom - sr.top;
    }
    contentHeight_ = (std::max)(0, height - toolbarHeight_ - statusHeight);
    const int availableWidth = (std::max)(0, width - kSplitterWidth);
    const int minimumWidth = (std::min)(kMinPaneWidth, availableWidth / 2);
    if (splitterPosition_ < 0) splitterPosition_ = width / 3;
    splitterPosition_ = std::clamp(splitterPosition_, minimumWidth, availableWidth - minimumWidth);
    MoveWindow(treeCtl_, 0, toolbarHeight_, splitterPosition_, contentHeight_, TRUE);
    const int rightLeft = splitterPosition_ + kSplitterWidth;
    const int rightWidth = availableWidth - splitterPosition_;
    const int buttonWidth = 30;
    const int controlTop = toolbarHeight_ + 3;
    MoveWindow(backCtl_, rightLeft + 3, controlTop, buttonWidth, 24, TRUE);
    MoveWindow(forwardCtl_, rightLeft + 37, controlTop, buttonWidth, 24, TRUE);
    MoveWindow(addressCtl_, rightLeft + 72, controlTop, (std::max)(0, rightWidth - 75), 24, TRUE);
    MoveWindow(filesCtl_, rightLeft, toolbarHeight_ + kNavigationHeight, rightWidth,
        (std::max)(0, contentHeight_ - kNavigationHeight), TRUE);
}

bool MainFrame::IsOverSplitter(int x, int y) const {
    return x >= splitterPosition_ && x < splitterPosition_ + kSplitterWidth &&
        y >= toolbarHeight_ && y < toolbarHeight_ + contentHeight_;
}

void MainFrame::OnDestroy() {
    if (worker_.joinable()) worker_.join();
    if (toolbarImages_) {
        ImageList_Destroy(toolbarImages_);
        toolbarImages_ = nullptr;
    }
    PostQuitMessage(0);
}

void MainFrame::OnExit() {
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

void MainFrame::OnAbout() {
    MessageBoxW(hwnd_, L"Where Is That?\nNative Win32 build.", L"About", MB_OK);
}

void MainFrame::OnCommand(int id) {
    if (id == ID_FILE_NEWCATALOG) OnNewCatalog();
    else if (id == ID_FILE_OPEN) OnOpenCatalog();
    else if (id >= ID_FILE_RECENT_FIRST && id <= ID_FILE_RECENT_LAST) OnOpenRecentCatalog(id);
    else if (id == ID_EDIT_ADDDISKIMAGE) OnAddOrUpdateDiskImage();
    else if (id == ID_SEARCH_FOR_ITEMS) OnSearchForItems();
    else if (id == IDC_BROWSER_BACK) NavigateBack();
    else if (id == IDC_BROWSER_FORWARD) NavigateForward();
    else if (id == ID_OPTIONS_GENERAL_SETTINGS) OnGeneralSettings();
    else if (id == ID_FILE_EXIT) OnExit();
    else if (id == ID_HELP_ABOUT) OnAbout();
}

void MainFrame::ClearCatalogViews() {
    tree_.Clear();
    currentLocation_ = {};
    history_.clear();
    historyIndex_ = -1;
    files_.SetLocation(currentLocation_, nullptr);
    SetWindowTextW(addressCtl_, L"");
    UpdateNavigationControls();
}

std::wstring MainFrame::CatalogLabel() const {
    return activeCatalogPath_.empty() ? L"" : NameForCatalogRoot(activeCatalogPath_);
}

std::wstring MainFrame::AddressFor(const wit::core::BrowserLocation& location) const {
    auto address = CatalogLabel();
    if (location.isRoot) return address;
    address += L"\\" + location.sourceName;
    if (location.path.size() > location.sourceRoot.size()) {
        auto relative = location.path.substr(location.sourceRoot.size());
        if (!relative.empty() && relative.front() != L'\\' && relative.front() != L'/') {
            address += L"\\";
        }
        address += relative;
    }
    return address;
}

void MainFrame::UpdateNavigationControls() {
    EnableWindow(backCtl_, historyIndex_ > 0);
    EnableWindow(forwardCtl_, historyIndex_ >= 0 &&
        historyIndex_ + 1 < static_cast<int>(history_.size()));
}

void MainFrame::NavigateTo(const wit::core::BrowserLocation& location, bool addToHistory) {
    if (!db_.IsOpen()) return;
    if (addToHistory) {
        if (historyIndex_ >= 0 && currentLocation_ == location) return;
        history_.resize(static_cast<std::size_t>(historyIndex_ + 1));
        history_.push_back(location);
        historyIndex_ = static_cast<int>(history_.size()) - 1;
    }
    currentLocation_ = location;
    files_.SetLocation(currentLocation_, &db_);
    const auto address = AddressFor(currentLocation_);
    SetWindowTextW(addressCtl_, address.c_str());
    selectingTree_ = true;
    tree_.SelectLocation(currentLocation_);
    selectingTree_ = false;
    UpdateNavigationControls();
}

void MainFrame::ReloadBrowser() {
    if (!db_.IsOpen()) {
        ClearCatalogViews();
        return;
    }
    const auto sources = db_.GetCatalogs();
    selectingTree_ = true;
    tree_.Reload(CatalogLabel(), sources, &db_);
    selectingTree_ = false;
    history_.clear();
    historyIndex_ = -1;
    NavigateTo({}, true);
}

void MainFrame::NavigateBack() {
    if (historyIndex_ <= 0) return;
    --historyIndex_;
    NavigateTo(history_[historyIndex_], false);
}

void MainFrame::NavigateForward() {
    if (historyIndex_ < 0 || historyIndex_ + 1 >= static_cast<int>(history_.size())) return;
    ++historyIndex_;
    NavigateTo(history_[historyIndex_], false);
}

LRESULT MainFrame::OnTreeSelectionChanged(LPNMHDR hdr) {
    if (selectingTree_) return 0;
    const auto* notification = reinterpret_cast<NMTREEVIEWW*>(hdr);
    const auto* location = tree_.LocationFor(notification->itemNew.hItem);
    if (location) NavigateTo(*location, true);
    return 0;
}

LRESULT MainFrame::OnTreeExpanding(LPNMHDR hdr) {
    const auto* notification = reinterpret_cast<NMTREEVIEWW*>(hdr);
    if (notification->action == TVE_EXPAND) tree_.Expand(notification->itemNew.hItem);
    return 0;
}

LRESULT MainFrame::OnFileGetDispInfo(LPNMHDR hdr) {
    auto* displayInfo = reinterpret_cast<NMLVDISPINFOW*>(hdr);
    if (displayInfo->item.mask & LVIF_TEXT) {
        auto text = files_.TextFor(displayInfo->item.iItem, displayInfo->item.iSubItem);
        lstrcpynW(displayInfo->item.pszText, text.c_str(), displayInfo->item.cchTextMax);
    }
    return 0;
}

LRESULT MainFrame::OnFileActivate(LPNMHDR hdr) {
    const auto* activation = reinterpret_cast<NMITEMACTIVATE*>(hdr);
    if (activation->iItem < 0) return 0;
    const auto* entry = files_.EntryAt(activation->iItem);
    if (!entry || !entry->isDirectory) return 0;

    wit::core::BrowserLocation next;
    next.isRoot = false;
    if (currentLocation_.isRoot) {
        next.sourceId = entry->catalogId;
        next.sourceName = entry->name;
        next.sourceRoot = entry->parentPath;
        next.path = entry->parentPath;
    } else {
        next = currentLocation_;
        next.path = JoinStoredPath(currentLocation_.path, entry->name);
    }
    NavigateTo(next, true);
    return 0;
}

LRESULT MainFrame::OnToolbarDropDown(LPNMTOOLBAR notification) {
    if (notification->iItem != ID_VIEW_DETAILS) return TBDDRET_NODEFAULT;
    RECT buttonRect{};
    if (!SendMessageW(toolbar_, TB_GETRECT, ID_VIEW_DETAILS, reinterpret_cast<LPARAM>(&buttonRect))) {
        return TBDDRET_NODEFAULT;
    }
    MapWindowPoints(toolbar_, HWND_DESKTOP, reinterpret_cast<LPPOINT>(&buttonRect), 2);
    const auto menu = CreatePopupMenu();
    if (!menu) return TBDDRET_NODEFAULT;
    AppendMenuW(menu, MF_STRING, ID_VIEW_LIST, L"View List");
    AppendMenuW(menu, MF_STRING, ID_VIEW_DETAILS, L"View Details");
    const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
        buttonRect.left, buttonRect.bottom, hwnd_, nullptr);
    DestroyMenu(menu);
    if (command != 0) PostMessageW(hwnd_, WM_COMMAND, command, 0);
    return TBDDRET_DEFAULT;
}

bool MainFrame::ActivateCatalog(const std::wstring& path, bool createNew, bool persistPath) {
    wit::storage::Database candidate;
    const bool opened = createNew ? candidate.CreateNew(path) : candidate.OpenExisting(path);
    if (!opened) return false;
    db_ = std::move(candidate);
    activeCatalogPath_ = path;
    ClearCatalogViews();
    ReloadBrowser();
    if (persistPath) {
        settings_.lastCatalogPath = path;
        wit::platform::RememberRecentCatalog(settings_, path);
        if (!wit::platform::SaveAppSettings(settings_)) {
            MessageBoxW(hwnd_, L"The catalog opened, but its path could not be saved in settings.ini.",
                L"Catalog Settings", MB_OK | MB_ICONWARNING);
        }
        RefreshOpenRecentMenu();
    }
    return true;
}

void MainFrame::OnNewCatalog() {
    if (worker_.joinable()) {
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring path;
    if (!ChooseNewCatalogPath(hwnd_, path)) return;
    if (!ActivateCatalog(path, true, true)) {
        MessageBoxW(hwnd_, L"Unable to create the new catalog. Choose a filename that does not already exist.",
            L"New Catalog", MB_OK | MB_ICONERROR);
        return;
    }
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Catalog created"));
}

void MainFrame::OnOpenCatalog() {
    if (worker_.joinable()) {
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring path;
    if (!ChooseCatalogToOpen(hwnd_, path)) return;
    if (!ActivateCatalog(path, false, true)) {
        MessageBoxW(hwnd_, L"The selected file is not an available catalog database.", L"Open Catalog",
            MB_OK | MB_ICONERROR);
        return;
    }
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Catalog opened"));
}

void MainFrame::OnOpenRecentCatalog(int commandId) {
    if (worker_.joinable()) {
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const auto index = static_cast<std::size_t>(commandId - ID_FILE_RECENT_FIRST);
    if (index >= settings_.recentCatalogPaths.size()) return;
    const auto path = settings_.recentCatalogPaths[index];
    if (!ActivateCatalog(path, false, true)) {
        MessageBoxW(hwnd_, L"The recent catalog is no longer available or is not a usable catalog database.",
            L"Open Recent Catalog", MB_OK | MB_ICONERROR);
        return;
    }
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Recent catalog opened"));
}

void MainFrame::RefreshOpenRecentMenu() {
    auto fileMenu = GetSubMenu(GetMenu(hwnd_), 0);
    auto recentMenu = fileMenu ? GetSubMenu(fileMenu, 2) : nullptr;
    if (!recentMenu) return;
    while (GetMenuItemCount(recentMenu) > 0) DeleteMenu(recentMenu, 0, MF_BYPOSITION);
    if (settings_.recentCatalogPaths.empty()) {
        AppendMenuW(recentMenu, MF_STRING | MF_GRAYED, ID_FILE_RECENT_FIRST, L"(No recent catalogs)");
    } else {
        const auto limit = (std::min)(settings_.recentCatalogPaths.size(),
            static_cast<std::size_t>(ID_FILE_RECENT_LAST - ID_FILE_RECENT_FIRST + 1));
        for (std::size_t index = 0; index < limit; ++index) {
            auto label = L"&" + std::to_wstring(index + 1) + L" " + settings_.recentCatalogPaths[index];
            for (std::size_t pos = label.find(L'&', 2); pos != std::wstring::npos;
                pos = label.find(L'&', pos + 2)) {
                label.insert(pos, 1, L'&');
            }
            AppendMenuW(recentMenu, MF_STRING, ID_FILE_RECENT_FIRST + static_cast<UINT>(index), label.c_str());
        }
    }
    DrawMenuBar(hwnd_);
}

void MainFrame::OnGeneralSettings() {
    GeneralSettingsDialogData data{settings_};
    const auto result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_GENERAL_SETTINGS),
        hwnd_, GeneralSettingsDialogProc, reinterpret_cast<LPARAM>(&data));
    if (result != IDOK) return;
    if (!wit::platform::SaveAppSettings(data.settings)) {
        MessageBoxW(hwnd_, L"Unable to save settings.ini.", L"General Settings", MB_OK | MB_ICONERROR);
        return;
    }
    settings_ = data.settings;
    ApplyDisplaySettings();
}

void MainFrame::OnSearchForItems() {
    if (!db_.IsOpen() || activeCatalogPath_.empty()) {
        MessageBoxW(hwnd_, L"Create or open a catalog before searching for items.", L"No Active Catalog",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    wit::ui::SearchDialog dialog;
    dialog.Show(hwnd_, GetModuleHandleW(nullptr), &db_);
}

void MainFrame::ApplyDisplaySettings() {
    ShowWindow(status_, settings_.showStatusBar ? SW_SHOW : SW_HIDE);
    RECT client{};
    GetClientRect(hwnd_, &client);
    OnSize(client.right - client.left, client.bottom - client.top);
}

void MainFrame::OnAddOrUpdateDiskImage() {
    if (worker_.joinable()) {
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!db_.IsOpen() || activeCatalogPath_.empty()) {
        MessageBoxW(hwnd_, L"Create or open a catalog before adding a disk image.", L"No Active Catalog",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    IFileOpenDialog* dialog{};
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Choose a folder or disk to scan");
    if (SUCCEEDED(dialog->Show(hwnd_))) {
        IShellItem* item{};
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path{};
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                std::wstring root = NormalizedSourcePath(path);
                CoTaskMemFree(path);
                SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Starting scan..."));
                const auto dbPath = activeCatalogPath_;
                worker_ = std::thread([this, root, dbPath]() {
                    wit::storage::Database scanDb;
                    std::int64_t id{};
                    bool success = scanDb.OpenExisting(dbPath) && scanDb.BeginTransaction();
                    if (success) {
                        id = scanDb.FindCatalogByRootPath(root);
                        if (id != 0) {
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
                    if (success) {
                        wit::core::FileScanner scanner;
                        success = scanner.ScanFolder(root, id, scanDb,
                            [this](const wit::core::FileScanner::Progress& progress) {
                                PostMessageW(hwnd_, WM_SCAN_PROGRESS, 0, reinterpret_cast<LPARAM>(
                                    new ScanProgressMessage{progress.scannedFiles, progress.scannedFolders}));
                            }, false);
                    }
                    if (success) {
                        if (!scanDb.Commit()) {
                            scanDb.Rollback();
                            success = false;
                        }
                    } else if (scanDb.IsOpen()) {
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
