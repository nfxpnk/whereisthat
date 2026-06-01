#include "wit_gui/MainWindow.h"
#include <algorithm>
#include <cwctype>
#include <optional>
#include <utility>
#include "wit_gui/AddDiskDialog.h"
#include "wit_gui/AboutDialog.h"
#include "wit_gui/CatalogFileDialog.h"
#include "wit_gui/GeneralSettingsDialog.h"
#include "wit_scanner/ScanRequest.h"
#include "resource.h"

namespace {

bool IsBlank(const std::wstring& value) {
    for (const auto character : value) {
        if (!iswspace(character)) return false;
    }
    return true;
}

INT_PTR CALLBACK DiskGroupDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* name = reinterpret_cast<std::wstring*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (message == WM_INITDIALOG) {
        SetWindowLongPtrW(dialog, GWLP_USERDATA, lparam);
        SetFocus(GetDlgItem(dialog, IDC_DISK_GROUP_NAME));
        return FALSE;
    }
    if (message != WM_COMMAND) return FALSE;
    if (LOWORD(wparam) == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    if (LOWORD(wparam) != IDOK || !name) return FALSE;
    const auto length = GetWindowTextLengthW(GetDlgItem(dialog, IDC_DISK_GROUP_NAME));
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    GetDlgItemTextW(dialog, IDC_DISK_GROUP_NAME, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(length));
    if (IsBlank(value)) {
        MessageBoxW(dialog, L"Disk group name is required.", L"Add New Disk Group", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(dialog, IDC_DISK_GROUP_NAME));
        return TRUE;
    }
    *name = std::move(value);
    EndDialog(dialog, IDOK);
    return TRUE;
}

bool PromptDiskGroupName(HWND owner, std::wstring& name) {
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_DISK_GROUP), owner,
        DiskGroupDialogProc, reinterpret_cast<LPARAM>(&name)) == IDOK;
}

wit::core::MediaSourceKind ToScanMediaSourceKind(wit::ui::MediaSourceKind kind) {
    switch (kind) {
    case wit::ui::MediaSourceKind::Drive:
        return wit::core::MediaSourceKind::Drive;
    case wit::ui::MediaSourceKind::Iso:
        return wit::core::MediaSourceKind::Iso;
    case wit::ui::MediaSourceKind::Folder:
    case wit::ui::MediaSourceKind::None:
    default:
        return wit::core::MediaSourceKind::Folder;
    }
}

wit::core::ScanRequest ToScanRequest(const wit::ui::AddNewDiskMediaResult& media) {
    return {
        media.destinationCatalogId,
        media.diskGroupId,
        ToScanMediaSourceKind(media.kind),
        media.scanRoot,
        media.originalPath,
        media.diskName,
        media.diskNumber,
        media.calculateCrc,
        media.browseArchives
    };
}

std::vector<wit::ui::CatalogChoice> ToDialogCatalogChoices(const std::vector<wit::app::CatalogChoice>& choices) {
    std::vector<wit::ui::CatalogChoice> dialogChoices;
    dialogChoices.reserve(choices.size());
    for (const auto& choice : choices) {
        wit::ui::CatalogChoice dialogChoice;
        dialogChoice.id = choice.id;
        dialogChoice.label = choice.label;
        dialogChoice.path = choice.path;
        for (const auto& group : choice.diskGroups) {
            dialogChoice.diskGroups.push_back({group.id, group.name});
        }
        dialogChoices.push_back(std::move(dialogChoice));
    }
    return dialogChoices;
}

}

bool MainFrame::Create() {
    const HMENU menu = LoadMenuW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDR_MAINMENU));
    if (WTL::CFrameWindowImpl<MainFrame>::Create(nullptr, ATL::CWindow::rcDefault, L"Where Is That?",
        WS_OVERLAPPEDWINDOW, 0, menu) == nullptr) {
        return false;
    }
    SetWindowPos(nullptr, 0, 0, 1100, 720, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    m_hAccel = LoadAcceleratorsW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDR_MAINACCEL));
    return true;
}

void MainFrame::Show(int command) {
    ShowWindow(command);
    UpdateWindow();
}

LRESULT MainFrame::OnFrameMessage(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
    return HandleMessage(message, wparam, lparam, handled);
}

LRESULT MainFrame::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled) {
    handled = TRUE;
    switch (message) {
    case WM_CREATE:
        return OnCreate() ? 0 : -1;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) chrome_.OnSize(LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_CLOSE:
        OnClose();
        return 0;
    case WM_LBUTTONDOWN:
        if (chrome_.OnLeftButtonDown(static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)))) {
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (chrome_.OnMouseMove(static_cast<short>(LOWORD(lparam)))) return 0;
        break;
    case WM_LBUTTONUP:
        if (chrome_.OnLeftButtonUp()) {
            controller_.SaveMainSplitterPosition(chrome_.SplitterPosition());
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        chrome_.OnCaptureChanged();
        break;
    case WM_SETCURSOR:
        if (chrome_.OnSetCursor(lparam)) return TRUE;
        break;
    case WM_COMMAND:
        OnCommand(LOWORD(wparam));
        return 0;
    case WM_DRAWITEM:
        if (chrome_.DrawStatusPart(reinterpret_cast<LPDRAWITEMSTRUCT>(lparam), protectedCatalog_)) return TRUE;
        break;
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<LPNMHDR>(lparam);
        if (header->idFrom == IDC_BROWSER_TREE && header->code == TVN_SELCHANGEDW) {
            ApplyControllerResult(controller_.SelectCatalog(browser_.OnTreeSelectionChanged(header)));
            return 0;
        }
        if (header->idFrom == IDC_BROWSER_TREE && header->code == TVN_ITEMEXPANDINGW) {
            return browser_.OnTreeExpanding(header);
        }
        if (header->idFrom == IDC_BROWSER_TREE && header->code == NM_RCLICK) return OnTreeRightClick();
        if (header->idFrom == IDC_FILES && header->code == LVN_GETDISPINFOW) {
            return browser_.OnFileGetDispInfo(header);
        }
        if (header->idFrom == IDC_FILES && header->code == LVN_ITEMACTIVATE) {
            const auto result = browser_.OnFileActivate(header);
            UpdateBrowserStatus();
            return result;
        }
        if (header->idFrom == IDC_FILES && header->code == LVN_ITEMCHANGED) {
            if (browser_.FileItemStateChanged(header)) UpdateBrowserStatus();
            return 0;
        }
        if (header->idFrom == IDC_TOOLBAR && header->code == TBN_DROPDOWN) {
            return chrome_.OnToolbarDropDown(reinterpret_cast<LPNMTOOLBAR>(header));
        }
        if (header->idFrom == IDC_TOOLBAR && header->code == TBN_GETINFOTIPW) {
            chrome_.SetToolbarTooltip(reinterpret_cast<LPNMTBGETINFOTIPW>(header));
            return 0;
        }
        break;
    }
    case wit::app::WM_SCAN_PROGRESS:
        ApplyControllerResult(controller_.OnScanProgress(static_cast<wit::app::ScanId>(wparam)));
        return 0;
    case wit::app::WM_SCAN_COMPLETE:
        ApplyControllerResult(controller_.OnScanComplete(static_cast<wit::app::ScanId>(wparam)));
        return 0;
    case WM_DESTROY:
        OnDestroy();
        return 0;
    }
    handled = FALSE;
    return 0;
}

bool MainFrame::OnCreate() {
    if (!controller_.AttachTarget(m_hWnd)) return false;
    auto initial = controller_.Initialize();
    if (!chrome_.Create(m_hWnd, initial.presentation.statusVisible, initial.presentation.toolbarVisible,
        initial.presentation.mainSplitterPosition, [this]() {
        browser_.SelectAll();
        UpdateBrowserStatus();
    })) return false;
    browser_.Attach(chrome_.TreeHandle(), chrome_.FilesHandle(), chrome_.BackHandle(),
        chrome_.ForwardHandle(), chrome_.AddressHandle(),
        [this](wit::core::CatalogId id) { return controller_.WorkingDatabase(id); },
        [this](wit::core::CatalogId id) { return controller_.CatalogLabel(id); });
    browser_.Clear();
    ApplyControllerResult(std::move(initial));
    chrome_.UpdateProgramStatusLights();
    return true;
}

void MainFrame::OnClose() {
    controller_.SaveMainSplitterPosition(chrome_.SplitterPosition());
    ApplyControllerResult(controller_.RequestWindowClose());
}

void MainFrame::OnDestroy() {
    controller_.SaveMainSplitterPosition(chrome_.SplitterPosition());
    searchDialog_.Close();
    scanProgressDialog_.Close();
    controller_.DetachTarget();
    chrome_.Destroy();
    PostQuitMessage(0);
}

void MainFrame::OnExit() {
    ::PostMessageW(m_hWnd, WM_CLOSE, 0, 0);
}

void MainFrame::OnAbout() {
    wit::ui::AboutDialog dialog;
    dialog.Show(m_hWnd);
}

void MainFrame::OnCommand(int id) {
    if (id == ID_FILE_NEWCATALOG) ApplyControllerResult(controller_.RequestNewCatalog());
    else if (id == ID_WIT_FILE_OPEN) ApplyControllerResult(controller_.RequestOpenCatalog());
    else if (id >= ID_FILE_RECENT_FIRST && id <= ID_FILE_RECENT_LAST) {
        ApplyControllerResult(controller_.RequestOpenRecentCatalog(
            static_cast<std::size_t>(id - ID_FILE_RECENT_FIRST)));
    } else if (id == ID_WIT_FILE_SAVE) ApplyControllerResult(controller_.RequestSave());
    else if (id == ID_WIT_FILE_CLOSE) ApplyControllerResult(controller_.RequestCloseCatalog());
    else if (id == ID_EDIT_ADDDISKIMAGE) ApplyControllerResult(controller_.RequestAddOrUpdateMedia());
    else if (id == ID_TREE_CONTEXT_ADD_NEW_DISK_GROUP_PLACEHOLDER) {
        std::wstring name;
        if (PromptDiskGroupName(m_hWnd, name)) ApplyControllerResult(controller_.CreateDiskGroup(name));
    }
    else if (id == ID_SEARCH_FOR_ITEMS) ApplyControllerResult(controller_.RequestSearch());
    else if (id == IDC_BROWSER_BACK) {
        browser_.NavigateBack();
        UpdateBrowserStatus();
    } else if (id == IDC_BROWSER_FORWARD) {
        browser_.NavigateForward();
        UpdateBrowserStatus();
    } else if (id == ID_OPTIONS_GENERAL_SETTINGS) {
        ApplyControllerResult(controller_.RequestGeneralSettings());
    } else if (id == ID_WIT_VIEW_TOOLBAR) {
        ApplyControllerResult(controller_.ToggleToolbar());
    } else if (id == ID_WIT_VIEW_STATUS_BAR) {
        ApplyControllerResult(controller_.ToggleStatusBar());
    } else if (id == ID_FILE_EXIT) OnExit();
    else if (id == ID_HELP_ABOUT) OnAbout();
}

LRESULT MainFrame::OnTreeRightClick() {
    POINT screenPoint{};
    GetCursorPos(&screenPoint);
    auto treePoint = screenPoint;
    ::ScreenToClient(chrome_.TreeHandle(), &treePoint);
    TVHITTESTINFO hitTest{};
    hitTest.pt = treePoint;
    const auto item = TreeView_HitTest(chrome_.TreeHandle(), &hitTest);
    if (!item) return 0;
    TreeView_SelectItem(chrome_.TreeHandle(), item);
    const auto menu = CreatePopupMenu();
    if (!menu) return 0;
    AppendMenuW(menu, MF_STRING, ID_EDIT_ADDDISKIMAGE, L"Add New Disk Image");
    AppendMenuW(menu, MF_STRING, ID_TREE_CONTEXT_ADD_NEW_DISK_GROUP_PLACEHOLDER, L"Add New Disk Group");
    AppendMenuW(menu, MF_STRING, ID_TREE_CONTEXT_UPDATE_ALL_DISK_IMAGES_PLACEHOLDER, L"Update All Disk Images");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_SEARCH_FIND_IN_CATALOG, L"Find in This Catalog");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_ACTIONS_EDIT_DESCRIPTION, L"Edit Description");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_FILE_NEWCATALOG, L"Add New Catalog");
    AppendMenuW(menu, MF_STRING, ID_WIT_FILE_OPEN, L"Open Catalog");
    AppendMenuW(menu, MF_STRING, ID_WIT_FILE_SAVE, L"Save Catalog");
    AppendMenuW(menu, MF_STRING, ID_FILE_REBUILD_DATABASE, L"Rebuild Catalog File");
    if (browser_.IsCatalogRoot(item)) AppendMenuW(menu, MF_STRING, ID_WIT_FILE_CLOSE, L"Close Catalog");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_EDIT_CATALOG_MANAGER, L"Catalog Manager");
    AppendMenuW(menu, MF_STRING, ID_EDIT_CATALOG_SETUP, L"Catalog Setup");
    AppendMenuW(menu, MF_STRING, ID_ACTIONS_PROPERTIES, L"Properties");
    const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
        screenPoint.x, screenPoint.y, m_hWnd, nullptr);
    DestroyMenu(menu);
    if (command) OnCommand(command);
    return 0;
}

void MainFrame::ApplyControllerResult(wit::app::ControllerResult result) {
    for (const auto& effect : result.browserEffects) {
        switch (effect.kind) {
        case wit::app::BrowserEffectKind::AddCatalog:
            browser_.AddCatalog(effect.catalogId, effect.label, effect.database, effect.select);
            break;
        case wit::app::BrowserEffectKind::RefreshCatalog:
            browser_.RefreshCatalog(effect.catalogId, effect.label, effect.database, effect.select);
            break;
        case wit::app::BrowserEffectKind::RemoveCatalog:
            browser_.RemoveCatalog(effect.catalogId);
            break;
        case wit::app::BrowserEffectKind::SelectCatalog:
            browser_.SelectCatalogRoot(effect.catalogId);
            break;
        case wit::app::BrowserEffectKind::Clear:
            browser_.Clear();
            break;
        }
    }
    RenderPresentation(result.presentation);
    switch (result.scanDialog.action) {
    case wit::app::ScanDialogAction::Show:
        scanProgressDialog_.Show(m_hWnd, [this]() {
            ApplyControllerResult(controller_.RequestCancelScan());
        });
        break;
    case wit::app::ScanDialogAction::Update:
        scanProgressDialog_.Update(result.scanDialog.files, result.scanDialog.folders, result.scanDialog.totalFiles,
            result.scanDialog.remainingFiles, result.scanDialog.totalKnown, result.scanDialog.counting);
        break;
    case wit::app::ScanDialogAction::Cancelling:
        scanProgressDialog_.SetCancelling();
        break;
    case wit::app::ScanDialogAction::Close:
        scanProgressDialog_.Close();
        break;
    case wit::app::ScanDialogAction::None:
        break;
    }
    for (const auto& message : result.messages) {
        ::MessageBoxW(m_hWnd, message.text.c_str(), message.title.c_str(), message.type);
    }
    if (result.request.kind != wit::app::RequestKind::None) PerformRequest(result.request);
    if (result.destroyWindow) DestroyWindow();
}

void MainFrame::PerformRequest(const wit::app::RequestEffect& request) {
    switch (request.kind) {
    case wit::app::RequestKind::ChooseNewCatalog: {
        std::wstring path;
        const wit::ui::CatalogFileDialog dialog;
        const bool accepted = dialog.ChooseNewCatalogPath(m_hWnd, path);
        ApplyControllerResult(controller_.CreateCatalogPathSelected(
            accepted ? std::optional<std::wstring>(path) : std::nullopt));
        break;
    }
    case wit::app::RequestKind::ChooseOpenCatalog: {
        std::wstring path;
        const wit::ui::CatalogFileDialog dialog;
        const bool accepted = dialog.ChooseCatalogToOpen(m_hWnd, path);
        ApplyControllerResult(controller_.OpenCatalogPathSelected(
            accepted ? std::optional<std::wstring>(path) : std::nullopt));
        break;
    }
    case wit::app::RequestKind::ConfirmCloseCatalog:
        ApplyControllerResult(controller_.AnswerCloseCatalog(::MessageBoxW(m_hWnd,
            L"Are you sure you want to close this catalog?", L"Close Catalog",
            MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION)));
        break;
    case wit::app::RequestKind::ConfirmPendingChanges:
        ApplyControllerResult(controller_.AnswerPendingChanges(::MessageBoxW(m_hWnd,
            L"Save changes?", L"Unsaved Catalog Changes", MB_YESNOCANCEL | MB_ICONWARNING)));
        break;
    case wit::app::RequestKind::ShowSearch: {
        if (!searchDialog_.Show(m_hWnd, request.database, [this]() {
            ApplyControllerResult(controller_.SearchClosed());
        })) {
            ApplyControllerResult(controller_.SearchClosed());
            ::MessageBoxW(m_hWnd, L"Unable to open the Search for Items window.",
                L"Search for Items", MB_OK | MB_ICONERROR);
        }
        break;
    }
    case wit::app::RequestKind::ShowAddOrUpdateMedia: {
        wit::ui::AddNewDiskMediaResult media;
        wit::ui::AddNewDiskMediaDialog dialog;
        const auto catalogChoices = ToDialogCatalogChoices(request.catalogChoices);
        const bool accepted = dialog.Show(m_hWnd, GetModuleHandleW(nullptr), catalogChoices,
            request.preferredCatalogId, media);
        ApplyControllerResult(controller_.MediaSelectionCompleted(
            accepted ? std::optional<wit::core::ScanRequest>(ToScanRequest(media)) : std::nullopt));
        break;
    }
    case wit::app::RequestKind::ShowGeneralSettings: {
        wit::ui::GeneralSettingsDialog dialog;
        wit::platform::AppSettings settings;
        const bool accepted = dialog.Show(m_hWnd, request.settings, settings);
        ApplyControllerResult(controller_.GeneralSettingsCompleted(
            accepted ? std::optional<wit::platform::AppSettings>(settings) : std::nullopt));
        break;
    }
    case wit::app::RequestKind::None:
        break;
    }
}

void MainFrame::RenderRecentMenu(const std::vector<std::wstring>& paths) {
    const auto fileMenu = GetSubMenu(::GetMenu(m_hWnd), 0);
    const auto recentMenu = fileMenu ? GetSubMenu(fileMenu, 2) : nullptr;
    if (!recentMenu) return;
    while (GetMenuItemCount(recentMenu) > 0) DeleteMenu(recentMenu, 0, MF_BYPOSITION);
    if (paths.empty()) {
        AppendMenuW(recentMenu, MF_STRING | MF_GRAYED, ID_FILE_RECENT_FIRST, L"(No recent catalogs)");
    } else {
        const auto limit = (std::min)(paths.size(),
            static_cast<std::size_t>(ID_FILE_RECENT_LAST - ID_FILE_RECENT_FIRST + 1));
        for (std::size_t index = 0; index < limit; ++index) {
            auto label = L"&" + std::to_wstring(index + 1) + L" " + paths[index];
            for (std::size_t pos = label.find(L'&', 2); pos != std::wstring::npos;
                pos = label.find(L'&', pos + 2)) label.insert(pos, 1, L'&');
            AppendMenuW(recentMenu, MF_STRING, ID_FILE_RECENT_FIRST + static_cast<UINT>(index), label.c_str());
        }
    }
    ::DrawMenuBar(m_hWnd);
}

void MainFrame::RenderPresentation(const wit::app::PresentationEffect& presentation) {
    protectedCatalog_ = presentation.protectedCatalog;
    chrome_.SetStatusText(0, presentation.catalogStatus);
    chrome_.UpdateCatalogLockStatus();
    chrome_.SetScanCommandEnabled(presentation.canScan);
    chrome_.SetSaveCommandEnabled(presentation.canSave);
    const auto menu = ::GetMenu(m_hWnd);
    if (menu) {
        ::EnableMenuItem(menu, ID_EDIT_ADDDISKIMAGE,
            MF_BYCOMMAND | (presentation.canScan ? MF_ENABLED : MF_GRAYED));
        ::EnableMenuItem(menu, ID_WIT_FILE_SAVE,
            MF_BYCOMMAND | (presentation.canSave ? MF_ENABLED : MF_GRAYED));
        ::EnableMenuItem(menu, ID_WIT_FILE_CLOSE,
            MF_BYCOMMAND | (presentation.canClose ? MF_ENABLED : MF_GRAYED));
        ::CheckMenuItem(menu, ID_WIT_VIEW_TOOLBAR,
            MF_BYCOMMAND | (presentation.toolbarVisible ? MF_CHECKED : MF_UNCHECKED));
        ::CheckMenuItem(menu, ID_WIT_VIEW_STATUS_BAR,
            MF_BYCOMMAND | (presentation.statusVisible ? MF_CHECKED : MF_UNCHECKED));
        ::DrawMenuBar(m_hWnd);
    }
    if (presentation.refreshBrowserStatus) UpdateBrowserStatus();
    if (presentation.refreshRecentMenu) RenderRecentMenu(presentation.recentCatalogPaths);
    if (presentation.updateStatusVisibility) chrome_.SetStatusVisible(presentation.statusVisible);
    if (presentation.updateToolbarVisibility) chrome_.SetToolbarVisible(presentation.toolbarVisible);
    if (presentation.updateAppStatus) chrome_.SetAppStatus(presentation.appStatus);
    if (presentation.flushStatus) ::UpdateWindow(chrome_.StatusHandle());
}

void MainFrame::UpdateBrowserStatus() {
    chrome_.SetStatusText(2, browser_.FocusedItemStatus());
    chrome_.SetStatusText(3, browser_.SelectionSummaryStatus());
}

