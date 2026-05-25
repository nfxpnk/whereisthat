#include "MainFrame.h"
#include <algorithm>
#include "../ui/AddNewDiskMediaDialog.h"
#include "../ui/CatalogFileDialog.h"
#include "../ui/GeneralSettingsDialog.h"
#include "../ui/SearchDialog.h"
#include "resource.h"

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
        if (chrome_.OnLeftButtonUp()) return 0;
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
        if (chrome_.DrawStatusPart(reinterpret_cast<LPDRAWITEMSTRUCT>(lparam),
            GetActiveCatalog() && !GetActiveCatalog()->IsEditable())) return TRUE;
        break;
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<LPNMHDR>(lparam);
        if (header->idFrom == IDC_BROWSER_TREE && header->code == TVN_SELCHANGEDW) {
            const auto catalogId = browser_.OnTreeSelectionChanged(header);
            if (catalogId) session_.SetActive(catalogId);
            RefreshCatalogStatus();
            RefreshCatalogCommands();
            RefreshBrowserStatus();
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
            RefreshBrowserStatus();
            return result;
        }
        if (header->idFrom == IDC_FILES && header->code == LVN_ITEMCHANGED) {
            if (browser_.FileItemStateChanged(header)) RefreshBrowserStatus();
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
        scans_.TakeProgress(static_cast<wit::app::ScanId>(wparam));
        return 0;
    case wit::app::WM_SCAN_COMPLETE: {
        const auto scanId = static_cast<wit::app::ScanId>(wparam);
        auto result = scans_.TakeResult(scanId);
        if (!result || scanId == 0 || scanId != activeScanId_) return 0;
        const bool cancellationRequested = scans_.IsCancelling();
        scans_.RetireWorker(scanId);
        activeScanId_ = 0;
        chrome_.SetAppStatus(wit::app::AppStatus::Idle);
        if (closePending_) {
            if (ConfirmAllPendingChanges()) {
                DestroyWindow();
            } else {
                closePending_ = false;
                SetScanControlsEnabled(true);
                RefreshCatalogStatus();
            }
            return 0;
        }
        SetScanControlsEnabled(true);
        if (cancellationRequested) {
            RefreshCatalogStatus();
            return 0;
        }
        if (result->outcome == wit::app::ScanOutcome::Completed && result->pending) {
            if (auto* catalog = session_.Find(result->destinationCatalogId)) {
                session_.AcceptPending(catalog->id, std::move(result->pending));
                const bool active = GetActiveCatalog() && GetActiveCatalog()->id == catalog->id;
                browser_.RefreshCatalog(catalog->id, catalog->label, catalog->WorkingDatabase(), active);
            }
            RefreshBrowserStatus();
        } else if (result->outcome == wit::app::ScanOutcome::Failed ||
            (result->outcome == wit::app::ScanOutcome::Completed && !result->pending)) {
            const auto message = result->error.empty()
                ? L"The scan could not be staged. The saved catalog was not changed." : result->error.c_str();
            ::MessageBoxW(m_hWnd, message, L"Add/Update Disk Image", MB_OK | MB_ICONERROR);
        }
        RefreshCatalogStatus();
        return 0;
    }
    case WM_DESTROY:
        OnDestroy();
        return 0;
    }
    handled = FALSE;
    return 0;
}

bool MainFrame::OnCreate() {
    if (!scans_.AttachTarget(m_hWnd)) return false;
    session_.LoadSettings();
    RefreshOpenRecentMenu();
    if (!chrome_.Create(m_hWnd, session_.Settings().showStatusBar, [this]() {
        browser_.SelectAll();
        RefreshBrowserStatus();
    })) return false;
    browser_.Attach(chrome_.TreeHandle(), chrome_.FilesHandle(), chrome_.BackHandle(),
        chrome_.ForwardHandle(), chrome_.AddressHandle(),
        [this](wit::core::CatalogId id) {
            auto* catalog = session_.Find(id);
            return catalog ? catalog->WorkingDatabase() : nullptr;
        },
        [this](wit::core::CatalogId id) {
            const auto* catalog = session_.Find(id);
            return catalog ? catalog->label : L"";
        });
    browser_.Clear();
    RefreshBrowserStatus();
    if (!session_.Settings().lastCatalogPath.empty() &&
        !ActivateCatalog(session_.Settings().lastCatalogPath, false, false)) {
        ::MessageBoxW(m_hWnd, L"The last used catalog is unavailable.", L"Open Catalog", MB_OK | MB_ICONINFORMATION);
    }
    RefreshCatalogStatus();
    RefreshCatalogCommands();
    chrome_.UpdateProgramStatusLights();
    return true;
}

void MainFrame::OnClose() {
    if (scans_.IsRunning()) {
        if (!closePending_) {
            closePending_ = true;
            scans_.RequestCancel();
            SetScanControlsEnabled(false);
        }
        return;
    }
    if (ConfirmAllPendingChanges()) DestroyWindow();
}

void MainFrame::OnDestroy() {
    scans_.DetachTarget();
    scans_.RequestCancel();
    chrome_.Destroy();
    PostQuitMessage(0);
}

void MainFrame::OnExit() {
    ::PostMessageW(m_hWnd, WM_CLOSE, 0, 0);
}

void MainFrame::OnAbout() {
    ::MessageBoxW(m_hWnd, L"Where Is That?\nNative Win32 build.", L"About", MB_OK);
}

void MainFrame::OnCommand(int id) {
    if (id == ID_FILE_NEWCATALOG) OnNewCatalog();
    else if (id == ID_WIT_FILE_OPEN) OnOpenCatalog();
    else if (id >= ID_FILE_RECENT_FIRST && id <= ID_FILE_RECENT_LAST) OnOpenRecentCatalog(id);
    else if (id == ID_WIT_FILE_SAVE) OnSaveCatalog();
    else if (id == ID_WIT_FILE_CLOSE) OnCloseCatalog();
    else if (id == ID_EDIT_ADDDISKIMAGE) OnAddOrUpdateDiskImage();
    else if (id == ID_SEARCH_FOR_ITEMS) OnSearchForItems();
    else if (id == IDC_BROWSER_BACK) {
        browser_.NavigateBack();
        RefreshBrowserStatus();
    } else if (id == IDC_BROWSER_FORWARD) {
        browser_.NavigateForward();
        RefreshBrowserStatus();
    } else if (id == ID_OPTIONS_GENERAL_SETTINGS) OnGeneralSettings();
    else if (id == ID_FILE_EXIT) OnExit();
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

bool MainFrame::ActivateCatalog(const std::wstring& path, bool createNew, bool persistPath) {
    bool settingsSaved{};
    bool alreadyOpen{};
    auto* catalog = session_.Open(path, createNew, persistPath, settingsSaved, alreadyOpen);
    if (!catalog) return false;
    if (alreadyOpen) browser_.SelectCatalogRoot(catalog->id);
    else browser_.AddCatalog(catalog->id, catalog->label, catalog->WorkingDatabase(), true);
    RefreshBrowserStatus();
    RefreshCatalogStatus();
    RefreshCatalogCommands();
    if (persistPath) {
        if (!settingsSaved) {
            ::MessageBoxW(m_hWnd, L"The catalog opened, but its path could not be saved in settings.ini.",
                L"Catalog Settings", MB_OK | MB_ICONWARNING);
        }
        RefreshOpenRecentMenu();
    }
    return true;
}

void MainFrame::OnNewCatalog() {
    if (scans_.IsRunning()) {
        ::MessageBoxW(m_hWnd, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring path;
    const wit::ui::CatalogFileDialog dialog;
    if (!dialog.ChooseNewCatalogPath(m_hWnd, path)) return;
    if (!ActivateCatalog(path, true, true)) {
        ::MessageBoxW(m_hWnd, L"Unable to create the new catalog. Choose a filename that does not already exist.",
            L"New Catalog", MB_OK | MB_ICONERROR);
    }
}

void MainFrame::OnOpenCatalog() {
    if (scans_.IsRunning()) {
        ::MessageBoxW(m_hWnd, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring path;
    const wit::ui::CatalogFileDialog dialog;
    if (!dialog.ChooseCatalogToOpen(m_hWnd, path)) return;
    if (!ActivateCatalog(path, false, true)) {
        ::MessageBoxW(m_hWnd, L"The selected file is not an available catalog database.", L"Open Catalog",
            MB_OK | MB_ICONERROR);
    }
}

void MainFrame::OnOpenRecentCatalog(int commandId) {
    if (scans_.IsRunning()) {
        ::MessageBoxW(m_hWnd, L"A scan is already running.", L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const auto index = static_cast<std::size_t>(commandId - ID_FILE_RECENT_FIRST);
    if (index >= session_.Settings().recentCatalogPaths.size()) return;
    const auto path = session_.Settings().recentCatalogPaths[index];
    if (!ActivateCatalog(path, false, true)) {
        ::MessageBoxW(m_hWnd, L"The recent catalog is no longer available or is not a usable catalog database.",
            L"Open Recent Catalog", MB_OK | MB_ICONERROR);
    }
}

void MainFrame::RefreshOpenRecentMenu() {
    const auto fileMenu = GetSubMenu(::GetMenu(m_hWnd), 0);
    const auto recentMenu = fileMenu ? GetSubMenu(fileMenu, 2) : nullptr;
    if (!recentMenu) return;
    while (GetMenuItemCount(recentMenu) > 0) DeleteMenu(recentMenu, 0, MF_BYPOSITION);
    const auto& paths = session_.Settings().recentCatalogPaths;
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

void MainFrame::OnGeneralSettings() {
    wit::ui::GeneralSettingsDialog dialog;
    wit::platform::AppSettings settings;
    if (!dialog.Show(m_hWnd, session_.Settings(), settings)) return;
    if (!session_.SaveSettings(settings)) {
        ::MessageBoxW(m_hWnd, L"Unable to save settings.ini.", L"General Settings", MB_OK | MB_ICONERROR);
        return;
    }
    chrome_.SetStatusVisible(session_.Settings().showStatusBar);
}

void MainFrame::OnSearchForItems() {
    auto* catalog = GetActiveCatalog();
    auto* database = catalog ? catalog->WorkingDatabase() : nullptr;
    if (!database || !database->IsOpen()) {
        ::MessageBoxW(m_hWnd, L"Create or open a catalog before searching for items.", L"No Active Catalog",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    chrome_.SetAppStatus(wit::app::AppStatus::Searching);
    wit::ui::SearchDialog dialog;
    dialog.Show(m_hWnd, database);
    chrome_.SetAppStatus(wit::app::AppStatus::Idle);
}

bool MainFrame::OnSaveCatalog() {
    auto* catalog = GetActiveCatalog();
    return !catalog || OnSaveCatalog(*catalog);
}

bool MainFrame::OnSaveCatalog(wit::app::OpenCatalog& catalog) {
    if (!catalog.IsOpen() || catalog.path.empty()) return true;
    if (scans_.Targets(catalog.id)) {
        ::MessageBoxW(m_hWnd, L"A scan is still preparing changes for this catalog.",
            L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    if (!catalog.IsEditable()) {
        ::MessageBoxW(m_hWnd, L"This catalog is protected or read-only and cannot be saved.",
            L"Protected Catalog", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    const bool hadPendingChanges = catalog.HasPendingChanges();
    if (!session_.SavePending(catalog.id)) {
        ::MessageBoxW(m_hWnd, L"Unable to save the pending catalog changes. They remain available to retry.",
            L"Save Catalog", MB_OK | MB_ICONERROR);
        RefreshCatalogStatus();
        return false;
    }
    if (hadPendingChanges) {
        browser_.RefreshCatalog(catalog.id, catalog.label, catalog.WorkingDatabase(),
            GetActiveCatalog() && GetActiveCatalog()->id == catalog.id);
        RefreshBrowserStatus();
    }
    RefreshCatalogStatus();
    return true;
}

void MainFrame::DiscardPendingChanges(wit::app::OpenCatalog& catalog) {
    session_.DiscardPending(catalog.id);
    browser_.RefreshCatalog(catalog.id, catalog.label, catalog.WorkingDatabase(),
        GetActiveCatalog() && GetActiveCatalog()->id == catalog.id);
    RefreshBrowserStatus();
    RefreshCatalogStatus();
}

bool MainFrame::ConfirmPendingChanges(wit::app::OpenCatalog& catalog) {
    if (!catalog.HasPendingChanges()) return true;
    const auto choice = ::MessageBoxW(m_hWnd,
        L"Save changes?",
        L"Unsaved Catalog Changes", MB_YESNOCANCEL | MB_ICONWARNING);
    if (choice == IDCANCEL) return false;
    if (choice == IDYES) return OnSaveCatalog(catalog);
    DiscardPendingChanges(catalog);
    return true;
}

bool MainFrame::ConfirmAllPendingChanges() {
    for (auto* catalog : session_.OpenCatalogs()) {
        if (!ConfirmPendingChanges(*catalog)) return false;
    }
    return true;
}

wit::app::OpenCatalog* MainFrame::GetActiveCatalog() {
    const auto selectedId = browser_.SelectedCatalogId();
    if (selectedId) session_.SetActive(selectedId);
    return session_.ActiveCatalog();
}

void MainFrame::OnCloseCatalog() {
    if (auto* catalog = GetActiveCatalog()) CloseCatalog(*catalog);
}

bool MainFrame::CloseCatalog(wit::app::OpenCatalog& catalog) {
    if (scans_.Targets(catalog.id)) {
        ::MessageBoxW(m_hWnd, L"A scan is still preparing changes for this catalog.",
            L"Scan in progress", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    const auto choice = ::MessageBoxW(m_hWnd, L"Are you sure you want to close this catalog?",
        L"Close Catalog", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION);
    if (choice != IDYES || !ConfirmPendingChanges(catalog)) return false;
    const auto id = catalog.id;
    browser_.RemoveCatalog(id);
    session_.Remove(id);
    if (auto* next = session_.ActiveCatalog()) {
        browser_.SelectCatalogRoot(next->id);
    } else {
        browser_.Clear();
    }
    RefreshBrowserStatus();
    RefreshCatalogStatus();
    RefreshCatalogCommands();
    return true;
}

void MainFrame::RefreshCatalogStatus() {
    const auto* catalog = GetActiveCatalog();
    chrome_.SetStatusText(0, !catalog ? L"No catalog" :
        (catalog->HasPendingChanges() ? L"Modified" : L"Loaded"));
    chrome_.UpdateCatalogLockStatus();
}

void MainFrame::RefreshBrowserStatus() {
    chrome_.SetStatusText(2, browser_.FocusedItemStatus());
    chrome_.SetStatusText(3, browser_.SelectionSummaryStatus());
}

void MainFrame::SetScanControlsEnabled(bool enabled) {
    (void)enabled;
    RefreshCatalogCommands();
}

void MainFrame::RefreshCatalogCommands() {
    auto* active = GetActiveCatalog();
    bool hasEditable{};
    for (const auto* catalog : session_.OpenCatalogs()) {
        if (catalog->IsEditable()) hasEditable = true;
    }
    const bool canScan = hasEditable && !scans_.IsRunning();
    const bool canSave = active && active->IsEditable() && !scans_.Targets(active->id);
    const bool canClose = active && !scans_.Targets(active->id);
    chrome_.SetScanCommandEnabled(canScan);
    chrome_.SetSaveCommandEnabled(canSave);
    const auto menu = ::GetMenu(m_hWnd);
    if (!menu) return;
    ::EnableMenuItem(menu, ID_EDIT_ADDDISKIMAGE, MF_BYCOMMAND | (canScan ? MF_ENABLED : MF_GRAYED));
    ::EnableMenuItem(menu, ID_WIT_FILE_SAVE, MF_BYCOMMAND | (canSave ? MF_ENABLED : MF_GRAYED));
    ::EnableMenuItem(menu, ID_WIT_FILE_CLOSE, MF_BYCOMMAND | (canClose ? MF_ENABLED : MF_GRAYED));
    ::DrawMenuBar(m_hWnd);
}

void MainFrame::OnAddOrUpdateDiskImage() {
    if (scans_.IsRunning()) {
        if (!scans_.IsCancelling()) {
            scans_.RequestCancel();
            ::MessageBoxW(m_hWnd, L"The active scan is being cancelled. Start the new scan after cancellation completes.",
                L"Cancelling scan", MB_OK | MB_ICONINFORMATION);
        } else {
            ::MessageBoxW(m_hWnd, L"Scan cancellation is still pending.",
                L"Cancelling scan", MB_OK | MB_ICONINFORMATION);
        }
        return;
    }
    std::vector<wit::ui::CatalogChoice> choices;
    for (const auto* catalog : session_.OpenCatalogs()) {
        if (catalog->IsEditable()) choices.push_back({catalog->id, catalog->label, catalog->path});
    }
    if (choices.empty()) {
        ::MessageBoxW(m_hWnd, L"Open an editable catalog before adding a disk image.", L"No Editable Catalog",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    wit::ui::AddNewDiskMediaResult media;
    wit::ui::AddNewDiskMediaDialog dialog;
    auto* active = GetActiveCatalog();
    const auto preferred = active && active->IsEditable() ? active->id : choices.front().id;
    if (!dialog.Show(m_hWnd, GetModuleHandleW(nullptr), choices, preferred, media)) return;
    auto* target = session_.Find(media.destinationCatalogId);
    if (!target || !target->IsEditable()) {
        ::MessageBoxW(m_hWnd, L"The selected destination catalog is no longer editable.",
            L"Add/Update Disk Image", MB_OK | MB_ICONWARNING);
        return;
    }
    chrome_.SetAppStatus(wit::app::AppStatus::Busy);
    ::UpdateWindow(chrome_.StatusHandle());
    wit::app::ScanId scanId{};
    if (!scans_.Start(target->WorkingDatabase(), media, scanId)) {
        chrome_.SetAppStatus(wit::app::AppStatus::Idle);
        ::MessageBoxW(m_hWnd, L"Unable to prepare pending catalog changes.",
            L"Add/Update Disk Image", MB_OK | MB_ICONERROR);
    } else {
        activeScanId_ = scanId;
        SetScanControlsEnabled(false);
    }
}
