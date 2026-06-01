#include <wit_gui/MainWindowChrome.h>
#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>
#include <wincodec.h>
#include "resource.h"

namespace wit::app {
namespace {
constexpr int kToolbarIconSize = 16;
constexpr int kToolbarButtonSize = 25;
constexpr int kNavigationHeight = 32;
constexpr int kCatalogStateStatusWidth = 88;
constexpr int kCatalogLockStatusWidth = 25;
constexpr int kSelectedItemsStatusMaxWidth = 456;
constexpr int kProgramStatusWidth = 38;
constexpr int kProgramStatusLightSize = 13;
constexpr int kProgramStatusLightSpacing = 1;

template<typename T>
void ReleaseIfPresent(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

int ToolbarScaleForDpi(HWND window) {
    const UINT dpi = GetDpiForWindow(window);
    return dpi > USER_DEFAULT_SCREEN_DPI ? 2 : 1;
}

HBITMAP LoadPngBitmap(IWICImagingFactory* factory, UINT resourceId, int targetSize) {
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
    IWICBitmapScaler* scaler{};
    IWICBitmapSource* source{};
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
            if (targetSize == kToolbarIconSize) {
                source = converter;
            } else if (SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
                SUCCEEDED(scaler->Initialize(converter, targetSize, targetSize,
                    WICBitmapInterpolationModeNearestNeighbor))) {
                source = scaler;
            }
            if (source) {
                BITMAPINFO info{};
                info.bmiHeader.biSize = sizeof(info.bmiHeader);
                info.bmiHeader.biWidth = targetSize;
                info.bmiHeader.biHeight = -targetSize;
                info.bmiHeader.biPlanes = 1;
                info.bmiHeader.biBitCount = 32;
                info.bmiHeader.biCompression = BI_RGB;
                bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
                if (!bitmap || FAILED(source->CopyPixels(nullptr, targetSize * 4,
                    targetSize * targetSize * 4, static_cast<BYTE*>(pixels)))) {
                    if (bitmap) DeleteObject(bitmap);
                    bitmap = nullptr;
                }
            }
        }
    }
    ReleaseIfPresent(scaler);
    ReleaseIfPresent(converter);
    ReleaseIfPresent(frame);
    ReleaseIfPresent(decoder);
    ReleaseIfPresent(stream);
    return bitmap;
}

const wchar_t* ToolbarTooltipText(int commandId) {
    switch (commandId) {
    case ID_FILE_NEWCATALOG: return L"Create New Catalog";
    case ID_WIT_FILE_OPEN: return L"Open Catalog";
    case ID_WIT_FILE_SAVE: return L"Save Current Catalog";
    case ID_FILE_REPORT_GENERATOR: return L"Generate Report";
    case ID_EDIT_ADDDISKIMAGE: return L"Add/Update Disk Image in Catalog";
    case ID_SEARCH_FOR_ITEMS: return L"Search";
    case ID_ACTIONS_EDIT_DESCRIPTION: return L"Edit Description";
    case ID_ACTIONS_PROPERTIES: return L"Item Properties";
    case ID_ACTIONS_OPEN_EXPLORER: return L"Open In Explorer";
    case ID_ACTIONS_VIEW_FILE: return L"View File";
    case ID_VIEW_DETAILS: return L"View Details";
    case ID_TOOLBAR_SORT_NAME: return L"Sort By Name";
    case ID_TOOLBAR_SORT_EXTENSION: return L"Sort By Extension";
    case ID_TOOLBAR_SORT_SIZE: return L"Sort By Size";
    case ID_TOOLBAR_SORT_DATE: return L"Sort By Date";
    case ID_TOOLBAR_SORT_REVERSE: return L"Reverse Sort Order";
    default: return L"";
    }
}
}

bool MainWindowChrome::Create(HWND parent, bool showStatusBar, int splitterPosition,
    std::function<void()> selectAllAction) {
    parent_ = parent;
    statusVisible_ = showStatusBar;
    splitterPosition_ = splitterPosition;
    selectAllAction_ = std::move(selectAllAction);
    if (!CreateToolbar()) return false;
    DWORD statusStyle = WS_CHILD;
    if (statusVisible_) statusStyle |= WS_VISIBLE;
    statusHandle_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, statusStyle,
        0, 0, 0, 0, parent_, nullptr, GetModuleHandleW(nullptr), nullptr);
    treeHandle_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, 0, 0, parent_, reinterpret_cast<HMENU>(IDC_BROWSER_TREE), GetModuleHandleW(nullptr), nullptr);
    backHandle_ = CreateWindowExW(0, L"BUTTON", L"<", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, parent_, reinterpret_cast<HMENU>(IDC_BROWSER_BACK), GetModuleHandleW(nullptr), nullptr);
    forwardHandle_ = CreateWindowExW(0, L"BUTTON", L">", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, parent_, reinterpret_cast<HMENU>(IDC_BROWSER_FORWARD), GetModuleHandleW(nullptr), nullptr);
    addressHandle_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY, 0, 0, 0, 0,
        parent_, reinterpret_cast<HMENU>(IDC_BROWSER_ADDRESS), GetModuleHandleW(nullptr), nullptr);
    filesHandle_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA, 0, 0, 0, 0,
        parent_, reinterpret_cast<HMENU>(IDC_FILES), GetModuleHandleW(nullptr), nullptr);
    if (!statusHandle_ || !treeHandle_ || !backHandle_ || !forwardHandle_ || !addressHandle_ || !filesHandle_) {
        return false;
    }
    TreeView_SetExtendedStyle(treeHandle_, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
    if (!CreateBrowserImages()) return false;
    ListView_SetExtendedListViewStyle(filesHandle_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    filesSubclass_.SetAction(&selectAllAction_);
    return filesSubclass_.SubclassWindow(filesHandle_) != FALSE;
}

bool MainWindowChrome::CreateToolbar() {
    const int toolbarScale = ToolbarScaleForDpi(parent_);
    const int toolbarIconSize = kToolbarIconSize * toolbarScale;
    const int toolbarButtonSize = kToolbarButtonSize * toolbarScale;
    toolbarHandle_ = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_TOP | CCS_NODIVIDER,
        0, 0, 0, 0, parent_, reinterpret_cast<HMENU>(IDC_TOOLBAR), GetModuleHandleW(nullptr), nullptr);
    if (!toolbarHandle_) return false;
    SendMessageW(toolbarHandle_, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(toolbarHandle_, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
    SendMessageW(toolbarHandle_, TB_SETBITMAPSIZE, 0, MAKELONG(toolbarIconSize, toolbarIconSize));
    SendMessageW(toolbarHandle_, TB_SETBUTTONSIZE, 0, MAKELONG(toolbarButtonSize, toolbarButtonSize));
    toolbarImages_ = ImageList_Create(toolbarIconSize, toolbarIconSize, ILC_COLOR32, 16, 0);
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
        const auto bitmap = LoadPngBitmap(factory, id, toolbarIconSize);
        if (!bitmap || ImageList_Add(toolbarImages_, bitmap, nullptr) == -1) {
            if (bitmap) DeleteObject(bitmap);
            factory->Release();
            return false;
        }
        DeleteObject(bitmap);
    }
    factory->Release();
    SendMessageW(toolbarHandle_, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(toolbarImages_));
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
    const auto addSeparator = [&buttons, toolbarScale]() {
        TBBUTTON separator{};
        separator.fsStyle = BTNS_SEP;
        separator.iBitmap = 8 * toolbarScale;
        buttons.push_back(separator);
    };
    addButton(0, ID_FILE_NEWCATALOG, BTNS_BUTTON);
    addButton(1, ID_WIT_FILE_OPEN, BTNS_BUTTON);
    addButton(2, ID_WIT_FILE_SAVE, BTNS_BUTTON);
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
    if (!SendMessageW(toolbarHandle_, TB_ADDBUTTONSW, static_cast<WPARAM>(buttons.size()),
        reinterpret_cast<LPARAM>(buttons.data()))) return false;
    SendMessageW(toolbarHandle_, TB_AUTOSIZE, 0, 0);
    return true;
}

bool MainWindowChrome::CreateBrowserImages() {
    browserImages_ = ImageList_Create(kToolbarIconSize, kToolbarIconSize, ILC_COLOR32, 105, 0);
    if (!browserImages_) return false;
    IWICImagingFactory* factory{};
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)))) return false;
    constexpr std::array<UINT, 5> baseImageIds = {
        IDB_BROWSER_FOLDER, IDB_BROWSER_DOCUMENT, IDB_BROWSER_ARCHIVE,
        IDB_BROWSER_DATABASE, IDB_BROWSER_DRIVE
    };
    for (const auto id : baseImageIds) {
        const auto bitmap = LoadPngBitmap(factory, id, kToolbarIconSize);
        if (!bitmap || ImageList_Add(browserImages_, bitmap, nullptr) == -1) {
            if (bitmap) DeleteObject(bitmap);
            factory->Release();
            return false;
        }
        DeleteObject(bitmap);
    }
    for (UINT id = IDB_BROWSER_FILE_TXT; id <= IDB_BROWSER_FILE_SH; ++id) {
        const auto bitmap = LoadPngBitmap(factory, id, kToolbarIconSize);
        if (!bitmap || ImageList_Add(browserImages_, bitmap, nullptr) == -1) {
            if (bitmap) DeleteObject(bitmap);
            factory->Release();
            return false;
        }
        DeleteObject(bitmap);
    }
    factory->Release();
    TreeView_SetImageList(treeHandle_, browserImages_, TVSIL_NORMAL);
    ListView_SetImageList(filesHandle_, browserImages_, LVSIL_SMALL);
    return true;
}

void MainWindowChrome::Destroy() {
    if (filesSubclass_.IsWindow()) {
        filesSubclass_.UnsubclassWindow(TRUE);
    }
    if (toolbarImages_) {
        ImageList_Destroy(toolbarImages_);
        toolbarImages_ = nullptr;
    }
    if (browserImages_) {
        ImageList_Destroy(browserImages_);
        browserImages_ = nullptr;
    }
}

void MainWindowChrome::OnSize(int width, int height) {
    if (toolbarHandle_) {
        SendMessageW(toolbarHandle_, TB_AUTOSIZE, 0, 0);
        RECT rect{};
        GetWindowRect(toolbarHandle_, &rect);
        toolbarHeight_ = rect.bottom - rect.top;
    }
    int statusHeight = 0;
    if (statusHandle_) {
        SendMessageW(statusHandle_, WM_SIZE, 0, 0);
        const int stateEnd = (std::min)(kCatalogStateStatusWidth, width);
        const int lockEnd = (std::min)(stateEnd + kCatalogLockStatusWidth, width);
        const int lightsStart = (std::max)(lockEnd, width - kProgramStatusWidth);
        const int selectedItemsWidth = (std::min)(kSelectedItemsStatusMaxWidth, lightsStart - lockEnd);
        const int selectionStart = lightsStart - selectedItemsWidth;
        const int parts[] = {stateEnd, lockEnd, selectionStart, lightsStart, -1};
        SendMessageW(statusHandle_, SB_SETPARTS, 5, reinterpret_cast<LPARAM>(parts));
        SendMessageW(statusHandle_, SB_SETTEXTW, 0 | SBT_OWNERDRAW, 0);
        SendMessageW(statusHandle_, SB_SETTEXTW, 1 | SBT_OWNERDRAW, 1);
        SendMessageW(statusHandle_, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(statusText_[2].c_str()));
        SendMessageW(statusHandle_, SB_SETTEXTW, 3 | SBT_OWNERDRAW, 3);
        SendMessageW(statusHandle_, SB_SETTEXTW, 4 | SBT_OWNERDRAW, 4);
        if (statusVisible_) {
            RECT rect{};
            GetWindowRect(statusHandle_, &rect);
            statusHeight = rect.bottom - rect.top;
        }
    }
    contentHeight_ = (std::max)(0, height - toolbarHeight_ - statusHeight);
    const int availableWidth = (std::max)(0, width - kSplitterWidth);
    const int minimumWidth = (std::min)(kMinPaneWidth, availableWidth / 2);
    if (splitterPosition_ < 0) splitterPosition_ = width / 3;
    splitterPosition_ = std::clamp(splitterPosition_, minimumWidth, availableWidth - minimumWidth);
    MoveWindow(treeHandle_, 0, toolbarHeight_, splitterPosition_, contentHeight_, TRUE);
    const int rightLeft = splitterPosition_ + kSplitterWidth;
    const int rightWidth = availableWidth - splitterPosition_;
    const int controlTop = toolbarHeight_ + 3;
    MoveWindow(backHandle_, rightLeft + 3, controlTop, 30, 24, TRUE);
    MoveWindow(forwardHandle_, rightLeft + 37, controlTop, 30, 24, TRUE);
    MoveWindow(addressHandle_, rightLeft + 72, controlTop, (std::max)(0, rightWidth - 75), 24, TRUE);
    MoveWindow(filesHandle_, rightLeft, toolbarHeight_ + kNavigationHeight, rightWidth,
        (std::max)(0, contentHeight_ - kNavigationHeight), TRUE);
}

bool MainWindowChrome::IsOverSplitter(int x, int y) const {
    return x >= splitterPosition_ && x < splitterPosition_ + kSplitterWidth &&
        y >= toolbarHeight_ && y < toolbarHeight_ + contentHeight_;
}

bool MainWindowChrome::OnLeftButtonDown(int x, int y) {
    if (!IsOverSplitter(x, y)) return false;
    splitterDragging_ = true;
    splitterDragOffset_ = x - splitterPosition_;
    SetCapture(parent_);
    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
    return true;
}

bool MainWindowChrome::OnMouseMove(int x) {
    if (!splitterDragging_) return false;
    RECT client{};
    GetClientRect(parent_, &client);
    splitterPosition_ = x - splitterDragOffset_;
    OnSize(client.right - client.left, client.bottom - client.top);
    return true;
}

bool MainWindowChrome::OnLeftButtonUp() {
    if (!splitterDragging_) return false;
    splitterDragging_ = false;
    if (GetCapture() == parent_) ReleaseCapture();
    return true;
}

void MainWindowChrome::OnCaptureChanged() {
    splitterDragging_ = false;
}

bool MainWindowChrome::OnSetCursor(LPARAM lparam) {
    POINT point{};
    GetCursorPos(&point);
    ScreenToClient(parent_, &point);
    if (!splitterDragging_ && (LOWORD(lparam) != HTCLIENT || !IsOverSplitter(point.x, point.y))) return false;
    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
    return true;
}

void MainWindowChrome::SetStatusVisible(bool visible) {
    statusVisible_ = visible;
    ShowWindow(statusHandle_, visible ? SW_SHOW : SW_HIDE);
    RECT client{};
    GetClientRect(parent_, &client);
    OnSize(client.right - client.left, client.bottom - client.top);
}

void MainWindowChrome::SetStatusText(int part, const std::wstring& text) {
    if (!statusHandle_ || part < 0 || part >= static_cast<int>(statusText_.size()) || statusText_[part] == text) {
        return;
    }
    statusText_[part] = text;
    if (part == 0 || part == 3) {
        InvalidateStatusPart(part);
        return;
    }
    SendMessageW(statusHandle_, SB_SETTEXTW, part, reinterpret_cast<LPARAM>(statusText_[part].c_str()));
}

void MainWindowChrome::UpdateCatalogLockStatus() {
    InvalidateStatusPart(1);
}

void MainWindowChrome::SetAppStatus(AppStatus status) {
    if (appStatus_ == status) return;
    appStatus_ = status;
    UpdateProgramStatusLights();
}

void MainWindowChrome::SetScanCommandEnabled(bool enabled) {
    if (toolbarHandle_) {
        SendMessageW(toolbarHandle_, TB_ENABLEBUTTON, ID_EDIT_ADDDISKIMAGE, MAKELPARAM(enabled, 0));
    }
}

void MainWindowChrome::SetSaveCommandEnabled(bool enabled) {
    if (toolbarHandle_) {
        SendMessageW(toolbarHandle_, TB_ENABLEBUTTON, ID_WIT_FILE_SAVE, MAKELPARAM(enabled, 0));
    }
}

void MainWindowChrome::UpdateProgramStatusLights() {
    InvalidateStatusPart(4);
}

void MainWindowChrome::InvalidateStatusPart(int part) {
    if (!statusHandle_) return;
    RECT rect{};
    if (SendMessageW(statusHandle_, SB_GETRECT, part, reinterpret_cast<LPARAM>(&rect))) {
        InvalidateRect(statusHandle_, &rect, FALSE);
        return;
    }
    InvalidateRect(statusHandle_, nullptr, TRUE);
}

bool MainWindowChrome::DrawStatusPart(LPDRAWITEMSTRUCT drawItem, bool protectedCatalog) {
    if (!drawItem || drawItem->hwndItem != statusHandle_) return false;
    FillRect(drawItem->hDC, &drawItem->rcItem, GetSysColorBrush(COLOR_BTNFACE));
    if (drawItem->itemData == 0) {
        RECT textRect = drawItem->rcItem;
        textRect.left += 6;
        textRect.right -= 6;
        SetBkMode(drawItem->hDC, TRANSPARENT);
        SetTextColor(drawItem->hDC, GetSysColor(COLOR_BTNTEXT));
        DrawTextW(drawItem->hDC, statusText_[0].c_str(), -1, &textRect,
            DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
    } else if (drawItem->itemData == 1 && protectedCatalog) {
        RECT body{drawItem->rcItem.left + 12, drawItem->rcItem.top + 9,
            drawItem->rcItem.left + 22, drawItem->rcItem.top + 17};
        Rectangle(drawItem->hDC, body.left, body.top, body.right, body.bottom);
        Arc(drawItem->hDC, body.left + 2, drawItem->rcItem.top + 4, body.right - 2, body.top + 5,
            body.left + 2, body.top, body.right - 2, body.top);
    } else if (drawItem->itemData == 4) {
        const int lightsWidth = kProgramStatusLightSize * 2 + kProgramStatusLightSpacing;
        const int left = drawItem->rcItem.left +
            ((drawItem->rcItem.right - drawItem->rcItem.left - lightsWidth) / 2);
        const int top = drawItem->rcItem.top +
            ((drawItem->rcItem.bottom - drawItem->rcItem.top - kProgramStatusLightSize) / 2);
        auto grey = CreateSolidBrush(RGB(150, 150, 150));
        auto green = CreateSolidBrush(RGB(47, 164, 73));
        const auto oldBrush = SelectObject(drawItem->hDC, grey);
        Ellipse(drawItem->hDC, left, top, left + kProgramStatusLightSize, top + kProgramStatusLightSize);
        SelectObject(drawItem->hDC, green);
        const int secondLeft = left + kProgramStatusLightSize + kProgramStatusLightSpacing;
        Ellipse(drawItem->hDC, secondLeft, top,
            secondLeft + kProgramStatusLightSize, top + kProgramStatusLightSize);
        SelectObject(drawItem->hDC, oldBrush);
        DeleteObject(grey);
        DeleteObject(green);
    } else if (drawItem->itemData == 3) {
        RECT textRect = drawItem->rcItem;
        textRect.left += 6;
        textRect.right -= 6;
        SetBkMode(drawItem->hDC, TRANSPARENT);
        SetTextColor(drawItem->hDC, GetSysColor(COLOR_BTNTEXT));
        DrawTextW(drawItem->hDC, statusText_[3].c_str(), -1, &textRect,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    }
    return true;
}

LRESULT MainWindowChrome::OnToolbarDropDown(LPNMTOOLBAR notification) {
    if (notification->iItem != ID_VIEW_DETAILS) return TBDDRET_NODEFAULT;
    RECT buttonRect{};
    if (!SendMessageW(toolbarHandle_, TB_GETRECT, ID_VIEW_DETAILS, reinterpret_cast<LPARAM>(&buttonRect))) {
        return TBDDRET_NODEFAULT;
    }
    MapWindowPoints(toolbarHandle_, HWND_DESKTOP, reinterpret_cast<LPPOINT>(&buttonRect), 2);
    const auto menu = CreatePopupMenu();
    if (!menu) return TBDDRET_NODEFAULT;
    AppendMenuW(menu, MF_STRING, ID_VIEW_LIST, L"View List");
    AppendMenuW(menu, MF_STRING, ID_VIEW_DETAILS, L"View Details");
    const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
        buttonRect.left, buttonRect.bottom, parent_, nullptr);
    DestroyMenu(menu);
    if (command != 0) PostMessageW(parent_, WM_COMMAND, command, 0);
    return TBDDRET_DEFAULT;
}

void MainWindowChrome::SetToolbarTooltip(LPNMTBGETINFOTIPW tip) const {
    lstrcpynW(tip->pszText, ToolbarTooltipText(tip->iItem), tip->cchTextMax);
}

}

