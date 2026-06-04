#include <wit_gui/CatalogWorkflowController.h>
#include <wit_infra/Logging.h>
#include <iterator>
#include <memory>
#include <format>
#include <utility>

namespace wit::app {
namespace {

MessageEffect Message(std::wstring text, std::wstring title, UINT type) {
    return {std::move(text), std::move(title), type};
}

}

bool CatalogWorkflowController::AttachTarget(HWND target) {
    return scans_.AttachTarget(target);
}

void CatalogWorkflowController::DetachTarget() {
    scans_.DetachTarget();
    // Best-effort shutdown cleanup; no UI result can be reported after detaching.
    (void)scans_.RequestCancel();
}

ControllerResult CatalogWorkflowController::Initialize() {
    session_.LoadSettings();
    ControllerResult result;
    if (!session_.SaveSettings(session_.Settings())) {
        result.messages.push_back(Message(L"Unable to save settings.ini.", L"Application Settings",
            MB_OK | MB_ICONWARNING));
    }
    result.presentation.updateStatusVisibility = true;
    result.presentation.statusVisible = session_.Settings().showStatusBar;
    result.presentation.updateToolbarVisibility = true;
    result.presentation.toolbarVisible = session_.Settings().showToolbar;
    result.presentation.mainSplitterPosition = session_.Settings().mainSplitterPosition;
    PopulatePresentation(result, true);
    if (!session_.Settings().lastCatalogPath.empty()) {
        Append(result, ActivateCatalog(session_.Settings().lastCatalogPath, false, false,
            L"Open Catalog", L"The last used catalog is unavailable.", MB_OK | MB_ICONINFORMATION));
    }
    return result;
}

ControllerResult CatalogWorkflowController::SelectCatalog(wit::core::CatalogId id) {
    ControllerResult result;
    if (id != 0 && !session_.SetActive(id)) {
        result.messages.push_back(Message(L"The selected catalog is no longer available.",
            L"Select Catalog", MB_OK | MB_ICONINFORMATION));
    }
    PopulatePresentation(result);
    result.presentation.refreshBrowserStatus = true;
    return result;
}

ControllerResult CatalogWorkflowController::RequestNewCatalog() {
    ControllerResult result;
    if (scans_.IsRunning()) {
        result.messages.push_back(Message(L"A scan is already running.", L"Scan in progress",
            MB_OK | MB_ICONINFORMATION));
    } else {
        result.request.kind = RequestKind::ChooseNewCatalog;
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::RequestOpenCatalog() {
    ControllerResult result;
    if (scans_.IsRunning()) {
        result.messages.push_back(Message(L"A scan is already running.", L"Scan in progress",
            MB_OK | MB_ICONINFORMATION));
    } else {
        result.request.kind = RequestKind::ChooseOpenCatalog;
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::RequestOpenRecentCatalog(std::size_t index) {
    ControllerResult result;
    if (scans_.IsRunning()) {
        result.messages.push_back(Message(L"A scan is already running.", L"Scan in progress",
            MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    if (index >= session_.Settings().recentCatalogPaths.size()) {
        PopulatePresentation(result);
        return result;
    }
    return ActivateCatalog(session_.Settings().recentCatalogPaths[index], false, true,
        L"Open Recent Catalog",
        L"The recent catalog is no longer available or is not a usable catalog database.",
        MB_OK | MB_ICONERROR);
}

ControllerResult CatalogWorkflowController::CreateCatalogPathSelected(const std::optional<std::wstring>& path) {
    if (!path) {
        ControllerResult result;
        PopulatePresentation(result);
        return result;
    }
    if (session_.IsPathOpen(*path)) {
        ControllerResult result;
        result.messages.push_back(Message(
            L"Unable to save the new catalog because this catalog is currently open in the application. "
            L"Choose a different filename that does not already exist or close the opened catalog.",
            L"New Catalog", MB_OK | MB_ICONERROR));
        PopulatePresentation(result);
        return result;
    }
    return ActivateCatalog(*path, true, true, L"New Catalog",
        L"Unable to save the new catalog at the selected filename.",
        MB_OK | MB_ICONERROR);
}

ControllerResult CatalogWorkflowController::OpenCatalogPathSelected(const std::optional<std::wstring>& path) {
    if (!path) {
        ControllerResult result;
        PopulatePresentation(result);
        return result;
    }
    return ActivateCatalog(*path, false, true, L"Open Catalog",
        L"The selected file is not an available catalog database.", MB_OK | MB_ICONERROR);
}

ControllerResult CatalogWorkflowController::ActivateCatalog(const std::wstring& path, bool createNew,
    bool persistPath, const std::wstring& failureTitle, const std::wstring& failureMessage, UINT failureType) {
    ControllerResult result;
    WIT_LOG_INFO(std::format(L"activate catalog path='{}' createNew={} persistPath={}",
        path, createNew, persistPath));
    bool settingsSaved{};
    bool alreadyOpen{};
    auto* catalog = session_.Open(path, createNew, persistPath, settingsSaved, alreadyOpen);
    if (!catalog) {
        WIT_LOG_ERROR(std::format(L"activate catalog failed path='{}'", path));
        result.messages.push_back(Message(failureMessage, failureTitle, failureType));
        PopulatePresentation(result);
        return result;
    }
    WIT_LOG_INFO(std::format(L"catalog active id={} label='{}' alreadyOpen={} editable={}",
        catalog->id, catalog->label, alreadyOpen, catalog->IsEditable()));
    if (alreadyOpen) {
        result.browserEffects.push_back({BrowserEffectKind::SelectCatalog, catalog->id});
    } else {
        result.browserEffects.push_back({BrowserEffectKind::AddCatalog, catalog->id, catalog->label,
            catalog->WorkingDatabase(), true});
    }
    if (persistPath && !settingsSaved) {
        result.messages.push_back(Message(
            L"The catalog opened, but its path could not be saved in settings.ini.",
            L"Catalog Settings", MB_OK | MB_ICONWARNING));
    }
    PopulatePresentation(result, persistPath);
    result.presentation.refreshBrowserStatus = true;
    return result;
}

ControllerResult CatalogWorkflowController::RequestSave() {
    const auto* catalog = ActiveCatalog();
    if (!catalog) {
        ControllerResult result;
        PopulatePresentation(result);
        return result;
    }
    return SaveCatalog(catalog->id);
}

ControllerResult CatalogWorkflowController::SaveCatalog(wit::core::CatalogId id) {
    ControllerResult result;
    WIT_LOG_INFO(std::format(L"save catalog requested id={}", id));
    auto* catalog = session_.Find(id);
    if (!catalog || !catalog->IsOpen() || catalog->path.empty()) {
        WIT_LOG_WARN(std::format(L"save catalog ignored: catalog unavailable id={}", id));
        PopulatePresentation(result);
        return result;
    }
    if (!catalog->HasPendingChanges()) {
        WIT_LOG_INFO(std::format(L"save catalog skipped: no pending changes id={}", id));
        PopulatePresentation(result);
        return result;
    }
    if (scans_.Targets(id)) {
        WIT_LOG_WARN(std::format(L"save catalog blocked by active scan id={}", id));
        result.messages.push_back(Message(L"A scan is still preparing changes for this catalog.",
            L"Scan in progress", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    if (!catalog->IsEditable()) {
        WIT_LOG_WARN(std::format(L"save catalog blocked by read-only catalog id={} path='{}'", id, catalog->path));
        result.messages.push_back(Message(L"This catalog is protected or read-only and cannot be saved.",
            L"Protected Catalog", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    const bool hadPendingChanges = catalog->HasPendingChanges();
    if (!session_.SavePending(id)) {
        WIT_LOG_ERROR(std::format(L"save catalog failed id={} path='{}'", id, catalog->path));
        result.messages.push_back(Message(
            L"Unable to save the pending catalog changes. They remain available to retry.",
            L"Save Catalog", MB_OK | MB_ICONERROR));
        PopulatePresentation(result);
        return result;
    }
    if (hadPendingChanges) {
        WIT_LOG_INFO(std::format(L"save catalog completed id={} path='{}'", id, catalog->path));
        result.browserEffects.push_back({BrowserEffectKind::RefreshCatalog, id, catalog->label,
            catalog->WorkingDatabase(), true});
        result.presentation.refreshBrowserStatus = true;
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::RequestCloseCatalog() {
    ControllerResult result;
    auto* catalog = ActiveCatalog();
    if (!catalog) {
        PopulatePresentation(result);
        return result;
    }
    if (scans_.Targets(catalog->id)) {
        result.messages.push_back(Message(L"A scan is still preparing changes for this catalog.",
            L"Scan in progress", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    promptCatalogId_ = catalog->id;
    pendingContinuation_ = PendingContinuation::CloseCatalog;
    result.request.kind = RequestKind::ConfirmCloseCatalog;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::AnswerCloseCatalog(int answer) {
    if (answer != IDYES) {
        promptCatalogId_ = 0;
        pendingContinuation_ = PendingContinuation::None;
        ControllerResult result;
        PopulatePresentation(result);
        return result;
    }
    return ContinuePendingWorkflow(promptCatalogId_, PendingContinuation::CloseCatalog);
}

ControllerResult CatalogWorkflowController::ContinuePendingWorkflow(wit::core::CatalogId id,
    PendingContinuation continuation) {
    ControllerResult result;
    auto* catalog = session_.Find(id);
    if (!catalog || !catalog->HasPendingChanges()) {
        return continuation == PendingContinuation::CloseCatalog
            ? ContinueCloseCatalog(id) : ContinueWindowClose();
    }
    promptCatalogId_ = id;
    pendingContinuation_ = continuation;
    result.request.kind = RequestKind::ConfirmPendingChanges;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::AnswerPendingChanges(int answer) {
    ControllerResult result;
    const auto id = promptCatalogId_;
    const auto continuation = pendingContinuation_;
    if (answer == IDCANCEL) {
        promptCatalogId_ = 0;
        pendingContinuation_ = PendingContinuation::None;
        closePending_ = false;
        PopulatePresentation(result);
        return result;
    }
    if (answer == IDYES) {
        auto saveResult = SaveCatalog(id);
        if (!saveResult.messages.empty()) return saveResult;
        if (continuation != PendingContinuation::CloseCatalog) Append(result, std::move(saveResult));
    } else {
        if (continuation == PendingContinuation::CloseCatalog) {
            session_.DiscardPending(id);
        } else {
            DiscardPending(id, result);
        }
    }
    promptCatalogId_ = 0;
    pendingContinuation_ = PendingContinuation::None;
    Append(result, continuation == PendingContinuation::CloseCatalog
        ? ContinueCloseCatalog(id) : ContinueWindowClose());
    return result;
}

void CatalogWorkflowController::DiscardPending(wit::core::CatalogId id, ControllerResult& result) {
    auto* catalog = session_.Find(id);
    if (!catalog) return;
    session_.DiscardPending(id);
    const bool active = ActiveCatalog() && ActiveCatalog()->id == id;
    result.browserEffects.push_back({BrowserEffectKind::RefreshCatalog, id, catalog->label,
        catalog->WorkingDatabase(), active});
    result.presentation.refreshBrowserStatus = true;
    PopulatePresentation(result);
}

ControllerResult CatalogWorkflowController::ContinueCloseCatalog(wit::core::CatalogId id) {
    ControllerResult result;
    if (!session_.Find(id)) {
        PopulatePresentation(result);
        return result;
    }
    bool settingsSaved{};
    if (!session_.Remove(id, &settingsSaved)) {
        PopulatePresentation(result);
        return result;
    }
    result.browserEffects.push_back({BrowserEffectKind::RemoveCatalog, id});
    if (const auto* next = session_.ActiveCatalog()) {
        result.browserEffects.push_back({BrowserEffectKind::SelectCatalog, next->id});
    } else {
        result.browserEffects.push_back({BrowserEffectKind::Clear});
    }
    if (!settingsSaved) {
        result.messages.push_back(Message(
            L"The catalog closed, but the startup catalog setting could not be saved in settings.ini.",
            L"Catalog Settings", MB_OK | MB_ICONWARNING));
    }
    result.presentation.refreshBrowserStatus = true;
    PopulatePresentation(result, settingsSaved);
    return result;
}

ControllerResult CatalogWorkflowController::RequestWindowClose() {
    ControllerResult result;
    if (scans_.IsRunning()) {
        if (!closePending_) {
            closePending_ = true;
            // Best-effort close flow; the UI is already moving into cancelling state.
            (void)scans_.RequestCancel();
        }
        result.scanDialog.action = ScanDialogAction::Cancelling;
        PopulatePresentation(result);
        return result;
    }
    return ContinueWindowClose();
}

ControllerResult CatalogWorkflowController::ContinueWindowClose() {
    ControllerResult result;
    for (auto* catalog : session_.OpenCatalogs()) {
        if (catalog->HasPendingChanges()) {
            return ContinuePendingWorkflow(catalog->id, PendingContinuation::CloseWindow);
        }
    }
    closePending_ = false;
    result.destroyWindow = true;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::RequestSearch() {
    ControllerResult result;
    auto* catalog = ActiveCatalog();
    auto* database = catalog ? catalog->WorkingDatabase() : nullptr;
    if (!database || !database->IsOpen()) {
        result.messages.push_back(Message(L"Create or open a catalog before searching for items.",
            L"No Active Catalog", MB_OK | MB_ICONINFORMATION));
    } else {
        result.request.kind = RequestKind::ShowSearch;
        result.request.database = database;
        result.presentation.updateAppStatus = true;
        result.presentation.appStatus = AppStatus::Searching;
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::SearchClosed() {
    ControllerResult result;
    result.presentation.updateAppStatus = true;
    result.presentation.appStatus = AppStatus::Idle;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::RequestAddOrUpdateMedia() {
    ControllerResult result;
    if (scans_.IsRunning()) {
        if (!scans_.IsCancelling()) {
            // Best-effort cancellation; the user-visible state below remains cancelling.
            (void)scans_.RequestCancel();
            result.messages.push_back(Message(
                L"The active scan is being cancelled. Start the new scan after cancellation completes.",
                L"Cancelling scan", MB_OK | MB_ICONINFORMATION));
        } else {
            result.messages.push_back(Message(L"Scan cancellation is still pending.",
                L"Cancelling scan", MB_OK | MB_ICONINFORMATION));
        }
        result.scanDialog.action = ScanDialogAction::Cancelling;
        PopulatePresentation(result);
        return result;
    }
    for (auto* catalog : session_.OpenCatalogs()) {
        if (catalog->IsEditable()) {
            result.request.catalogChoices.push_back(
                {catalog->id, catalog->label, catalog->path, catalog->WorkingDatabase()->GetDiskGroups()});
        }
    }
    if (result.request.catalogChoices.empty()) {
        result.messages.push_back(Message(L"Open an editable catalog before adding a disk image.",
            L"No Editable Catalog", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    const auto* active = ActiveCatalog();
    result.request.kind = RequestKind::ShowAddOrUpdateMedia;
    result.request.preferredCatalogId = active && active->IsEditable()
        ? active->id : result.request.catalogChoices.front().id;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::CreateDiskGroup(const std::wstring& name) {
    ControllerResult result;
    auto* catalog = ActiveCatalog();
    auto* database = catalog ? catalog->WorkingDatabase() : nullptr;
    WIT_LOG_INFO(std::format(L"create disk group requested catalogId={} name='{}'",
        catalog ? catalog->id : 0, name));
    if (!database || !database->IsEditable()) {
        result.messages.push_back(Message(L"Open an editable catalog before adding a disk group.",
            L"Add New Disk Group", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    auto pending = std::make_unique<wit::storage::Database>();
    if (!pending->CreateWorkingCopy(*database)) {
        result.messages.push_back(Message(L"Unable to prepare pending catalog changes.",
            L"Add New Disk Group", MB_OK | MB_ICONERROR));
        PopulatePresentation(result);
        return result;
    }
    const auto id = pending->CreateDiskGroup(name);
    if (id == 0) {
        WIT_LOG_WARN(std::format(L"create disk group failed catalogId={} name='{}'", catalog->id, name));
        result.messages.push_back(Message(L"Unable to create the disk group. Check that the name is unique.",
            L"Add New Disk Group", MB_OK | MB_ICONWARNING));
        PopulatePresentation(result);
        return result;
    }
    session_.AcceptPending(catalog->id, std::move(pending));
    WIT_LOG_INFO(std::format(L"create disk group staged catalogId={} groupId={} name='{}'",
        catalog->id, id, name));
    result.browserEffects.push_back({BrowserEffectKind::RefreshCatalog, catalog->id, catalog->label,
        catalog->WorkingDatabase(), true});
    result.presentation.refreshBrowserStatus = true;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::MoveDiskToGroup(wit::core::CatalogId catalogId,
    std::int64_t diskId, std::int64_t diskGroupId) {
    ControllerResult result;
    WIT_LOG_INFO(std::format(L"move disk requested catalogId={} diskId={} targetGroupId={}",
        catalogId, diskId, diskGroupId));
    auto* catalog = session_.Find(catalogId);
    auto* database = catalog ? catalog->WorkingDatabase() : nullptr;
    if (!database || !database->IsEditable()) {
        result.messages.push_back(Message(L"Open an editable catalog before moving a disk image.",
            L"Move to Group", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    if (scans_.Targets(catalogId)) {
        result.messages.push_back(Message(L"A scan is still preparing changes for this catalog.",
            L"Scan in progress", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    auto pending = std::make_unique<wit::storage::Database>();
    if (!pending->CreateWorkingCopy(*database)) {
        result.messages.push_back(Message(L"Unable to prepare pending catalog changes.",
            L"Move to Group", MB_OK | MB_ICONERROR));
        PopulatePresentation(result);
        return result;
    }
    if (!pending->MoveDiskToGroup(diskId, diskGroupId)) {
        WIT_LOG_WARN(std::format(L"move disk failed catalogId={} diskId={} targetGroupId={}",
            catalogId, diskId, diskGroupId));
        result.messages.push_back(Message(L"Unable to move the disk image to the selected group.",
            L"Move to Group", MB_OK | MB_ICONWARNING));
        PopulatePresentation(result);
        return result;
    }
    session_.AcceptPending(catalogId, std::move(pending));
    WIT_LOG_INFO(std::format(L"move disk staged catalogId={} diskId={} targetGroupId={}",
        catalogId, diskId, diskGroupId));
    const bool active = ActiveCatalog() && ActiveCatalog()->id == catalogId;
    result.browserEffects.push_back({BrowserEffectKind::MoveDiskToGroup, catalogId, catalog->label,
        catalog->WorkingDatabase(), active, diskId, diskGroupId});
    result.presentation.refreshBrowserStatus = true;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::MoveDiskGroupToGroup(wit::core::CatalogId catalogId,
    std::int64_t diskGroupId, std::int64_t parentGroupId) {
    ControllerResult result;
    WIT_LOG_INFO(std::format(L"move disk group requested catalogId={} groupId={} targetParentGroupId={}",
        catalogId, diskGroupId, parentGroupId));
    auto* catalog = session_.Find(catalogId);
    auto* database = catalog ? catalog->WorkingDatabase() : nullptr;
    if (!database || !database->IsEditable()) {
        result.messages.push_back(Message(L"Open an editable catalog before moving a disk group.",
            L"Move to Group", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    if (scans_.Targets(catalogId)) {
        result.messages.push_back(Message(L"A scan is still preparing changes for this catalog.",
            L"Scan in progress", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    auto pending = std::make_unique<wit::storage::Database>();
    if (!pending->CreateWorkingCopy(*database)) {
        result.messages.push_back(Message(L"Unable to prepare pending catalog changes.",
            L"Move to Group", MB_OK | MB_ICONERROR));
        PopulatePresentation(result);
        return result;
    }
    if (!pending->MoveDiskGroupToGroup(diskGroupId, parentGroupId)) {
        WIT_LOG_WARN(std::format(L"move disk group failed catalogId={} groupId={} targetParentGroupId={}",
            catalogId, diskGroupId, parentGroupId));
        result.messages.push_back(Message(L"Unable to move the disk group to the selected destination.",
            L"Move to Group", MB_OK | MB_ICONWARNING));
        PopulatePresentation(result);
        return result;
    }
    session_.AcceptPending(catalogId, std::move(pending));
    WIT_LOG_INFO(std::format(L"move disk group staged catalogId={} groupId={} targetParentGroupId={}",
        catalogId, diskGroupId, parentGroupId));
    const bool active = ActiveCatalog() && ActiveCatalog()->id == catalogId;
    result.browserEffects.push_back({BrowserEffectKind::RefreshCatalog, catalogId, catalog->label,
        catalog->WorkingDatabase(), active});
    result.presentation.refreshBrowserStatus = true;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::MediaSelectionCompleted(
    const std::optional<wit::core::ScanRequest>& request) {
    ControllerResult result;
    if (!request) {
        PopulatePresentation(result);
        return result;
    }
    auto* target = session_.Find(request->destinationCatalogId);
    if (!target || !target->IsEditable()) {
        result.messages.push_back(Message(L"The selected destination catalog is no longer editable.",
            L"Add/Update Disk Image", MB_OK | MB_ICONWARNING));
        PopulatePresentation(result);
        return result;
    }
    ScanId scanId{};
    result.presentation.updateAppStatus = true;
    result.presentation.appStatus = AppStatus::Busy;
    result.presentation.flushStatus = true;
    if (!scans_.Start(target->WorkingDatabase(), *request, scanId)) {
        result.presentation.appStatus = AppStatus::Idle;
        result.messages.push_back(Message(L"Unable to prepare pending catalog changes.",
            L"Add/Update Disk Image", MB_OK | MB_ICONERROR));
    } else {
        activeScanId_ = scanId;
        result.scanDialog.action = ScanDialogAction::Show;
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::RequestCancelScan() {
    ControllerResult result;
    if (scans_.IsRunning()) {
        if (!scans_.IsCancelling()) {
            // Best-effort cancellation; the scan dialog is already entering cancelling state.
            (void)scans_.RequestCancel();
        }
        result.scanDialog.action = ScanDialogAction::Cancelling;
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::RequestGeneralSettings() {
    ControllerResult result;
    result.request.kind = RequestKind::ShowGeneralSettings;
    result.request.settings = session_.Settings();
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::GeneralSettingsCompleted(
    const std::optional<wit::platform::AppSettings>& settings) {
    ControllerResult result;
    if (settings) {
        if (!session_.SaveSettings(*settings)) {
            result.messages.push_back(Message(L"Unable to save settings.ini.", L"General Settings",
                MB_OK | MB_ICONERROR));
        } else {
            result.presentation.updateStatusVisibility = true;
            result.presentation.statusVisible = session_.Settings().showStatusBar;
            result.presentation.updateToolbarVisibility = true;
            result.presentation.toolbarVisible = session_.Settings().showToolbar;
            result.presentation.refreshBrowserStatus = true;
        }
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::ToggleToolbar() {
    ControllerResult result;
    auto settings = session_.Settings();
    settings.showToolbar = !settings.showToolbar;
    if (!session_.SaveSettings(settings)) {
        result.messages.push_back(Message(L"Unable to save settings.ini.", L"Toolbar Settings",
            MB_OK | MB_ICONERROR));
    } else {
        result.presentation.updateToolbarVisibility = true;
        result.presentation.toolbarVisible = session_.Settings().showToolbar;
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::ToggleStatusBar() {
    ControllerResult result;
    auto settings = session_.Settings();
    settings.showStatusBar = !settings.showStatusBar;
    if (!session_.SaveSettings(settings)) {
        result.messages.push_back(Message(L"Unable to save settings.ini.", L"Status Bar Settings",
            MB_OK | MB_ICONERROR));
    } else {
        result.presentation.updateStatusVisibility = true;
        result.presentation.statusVisible = session_.Settings().showStatusBar;
    }
    PopulatePresentation(result);
    return result;
}

bool CatalogWorkflowController::SaveMainSplitterPosition(int position) {
    auto settings = session_.Settings();
    if (settings.mainSplitterPosition == position) return true;
    settings.mainSplitterPosition = position;
    return session_.SaveSettings(settings);
}

ControllerResult CatalogWorkflowController::OnScanProgress(ScanId scanId) {
    ControllerResult result;
    if (scanId != 0 && scanId == activeScanId_) {
        if (const auto progress = scans_.TakeProgress(scanId)) {
            result.scanDialog = {ScanDialogAction::Update, progress->files, progress->folders,
                progress->totalFiles, progress->remainingFiles, progress->totalKnown, progress->counting};
        }
    } else {
        // Drain stale progress for a scan that is no longer active.
        (void)scans_.TakeProgress(scanId);
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::OnScanComplete(ScanId scanId) {
    ControllerResult result;
    auto scanResult = scans_.TakeResult(scanId);
    if (!scanResult || scanId == 0 || scanId != activeScanId_) {
        PopulatePresentation(result);
        return result;
    }
    const bool cancellationRequested = scans_.IsCancelling();
    scans_.RetireWorker(scanId);
    activeScanId_ = 0;
    result.scanDialog.action = ScanDialogAction::Close;
    result.presentation.updateAppStatus = true;
    result.presentation.appStatus = AppStatus::Idle;
    if (closePending_) {
        closePending_ = false;
        Append(result, ContinueWindowClose());
        return result;
    }
    if (!cancellationRequested && scanResult->outcome == ScanOutcome::Completed && scanResult->pending) {
        if (auto* catalog = session_.Find(scanResult->destinationCatalogId)) {
            session_.AcceptPending(catalog->id, std::move(scanResult->pending));
            const bool active = ActiveCatalog() && ActiveCatalog()->id == catalog->id;
            result.browserEffects.push_back({BrowserEffectKind::RefreshCatalog, catalog->id, catalog->label,
                catalog->WorkingDatabase(), active});
            result.presentation.refreshBrowserStatus = true;
        }
    } else if (!cancellationRequested && (scanResult->outcome == ScanOutcome::Failed ||
        (scanResult->outcome == ScanOutcome::Completed && !scanResult->pending))) {
        result.messages.push_back(Message(scanResult->error.empty()
            ? L"The scan could not be staged. The saved catalog was not changed." : scanResult->error,
            L"Add/Update Disk Image", MB_OK | MB_ICONERROR));
    }
    PopulatePresentation(result);
    return result;
}

wit::storage::Database* CatalogWorkflowController::WorkingDatabase(wit::core::CatalogId id) {
    auto* catalog = session_.Find(id);
    return catalog ? catalog->WorkingDatabase() : nullptr;
}

std::wstring CatalogWorkflowController::CatalogLabel(wit::core::CatalogId id) const {
    const auto* catalog = session_.Find(id);
    return catalog ? catalog->label : L"";
}

void CatalogWorkflowController::PopulatePresentation(ControllerResult& result, bool refreshRecentMenu) {
    const auto* active = session_.ActiveCatalog();
    result.presentation.catalogStatus = !active ? L"No catalog" :
        (active->HasPendingChanges() ? L"Modified" : L"Loaded");
    result.presentation.protectedCatalog = active && !active->IsEditable();
    bool hasEditable{};
    for (const auto* catalog : session_.OpenCatalogs()) {
        if (catalog->IsEditable()) hasEditable = true;
    }
    result.presentation.canScan = hasEditable && !scans_.IsRunning();
    result.presentation.canSave = active && active->IsEditable() && active->HasPendingChanges() &&
        !scans_.Targets(active->id);
    result.presentation.canClose = active && !scans_.Targets(active->id);
    result.presentation.statusVisible = session_.Settings().showStatusBar;
    result.presentation.toolbarVisible = session_.Settings().showToolbar;
    if (refreshRecentMenu) {
        result.presentation.refreshRecentMenu = true;
        result.presentation.recentCatalogPaths = session_.Settings().recentCatalogPaths;
    }
}

void CatalogWorkflowController::Append(ControllerResult& destination, ControllerResult source) {
    destination.browserEffects.insert(destination.browserEffects.end(),
        std::make_move_iterator(source.browserEffects.begin()), std::make_move_iterator(source.browserEffects.end()));
    destination.messages.insert(destination.messages.end(),
        std::make_move_iterator(source.messages.begin()), std::make_move_iterator(source.messages.end()));
    if (source.request.kind != RequestKind::None) destination.request = std::move(source.request);
    if (source.presentation.refreshBrowserStatus) destination.presentation.refreshBrowserStatus = true;
    if (source.presentation.refreshRecentMenu) {
        destination.presentation.refreshRecentMenu = true;
        destination.presentation.recentCatalogPaths = std::move(source.presentation.recentCatalogPaths);
    }
    if (source.presentation.updateStatusVisibility) {
        destination.presentation.updateStatusVisibility = true;
        destination.presentation.statusVisible = source.presentation.statusVisible;
    }
    if (source.presentation.updateToolbarVisibility) {
        destination.presentation.updateToolbarVisibility = true;
        destination.presentation.toolbarVisible = source.presentation.toolbarVisible;
    }
    if (source.presentation.updateAppStatus) {
        destination.presentation.updateAppStatus = true;
        destination.presentation.appStatus = source.presentation.appStatus;
        destination.presentation.flushStatus = source.presentation.flushStatus;
    }
    destination.presentation.catalogStatus = std::move(source.presentation.catalogStatus);
    destination.presentation.protectedCatalog = source.presentation.protectedCatalog;
    destination.presentation.canScan = source.presentation.canScan;
    destination.presentation.canSave = source.presentation.canSave;
    destination.presentation.canClose = source.presentation.canClose;
    destination.destroyWindow = destination.destroyWindow || source.destroyWindow;
}

OpenCatalog* CatalogWorkflowController::ActiveCatalog() {
    return session_.ActiveCatalog();
}

}
