#include "CatalogWorkflowController.h"
#include <iterator>
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
    scans_.RequestCancel();
}

ControllerResult CatalogWorkflowController::Initialize() {
    session_.LoadSettings();
    ControllerResult result;
    result.presentation.updateStatusVisibility = true;
    result.presentation.statusVisible = session_.Settings().showStatusBar;
    PopulatePresentation(result, true);
    if (!session_.Settings().lastCatalogPath.empty()) {
        Append(result, ActivateCatalog(session_.Settings().lastCatalogPath, false, false,
            L"Open Catalog", L"The last used catalog is unavailable.", MB_OK | MB_ICONINFORMATION));
    }
    return result;
}

ControllerResult CatalogWorkflowController::SelectCatalog(wit::core::CatalogId id) {
    ControllerResult result;
    if (id != 0) session_.SetActive(id);
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
    return ActivateCatalog(*path, true, true, L"New Catalog",
        L"Unable to create the new catalog. Choose a filename that does not already exist.",
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
    bool settingsSaved{};
    bool alreadyOpen{};
    auto* catalog = session_.Open(path, createNew, persistPath, settingsSaved, alreadyOpen);
    if (!catalog) {
        result.messages.push_back(Message(failureMessage, failureTitle, failureType));
        PopulatePresentation(result);
        return result;
    }
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
    auto* catalog = session_.Find(id);
    if (!catalog || !catalog->IsOpen() || catalog->path.empty()) {
        PopulatePresentation(result);
        return result;
    }
    if (scans_.Targets(id)) {
        result.messages.push_back(Message(L"A scan is still preparing changes for this catalog.",
            L"Scan in progress", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    if (!catalog->IsEditable()) {
        result.messages.push_back(Message(L"This catalog is protected or read-only and cannot be saved.",
            L"Protected Catalog", MB_OK | MB_ICONINFORMATION));
        PopulatePresentation(result);
        return result;
    }
    const bool hadPendingChanges = catalog->HasPendingChanges();
    if (!session_.SavePending(id)) {
        result.messages.push_back(Message(
            L"Unable to save the pending catalog changes. They remain available to retry.",
            L"Save Catalog", MB_OK | MB_ICONERROR));
        PopulatePresentation(result);
        return result;
    }
    if (hadPendingChanges) {
        const bool active = ActiveCatalog() && ActiveCatalog()->id == id;
        result.browserEffects.push_back({BrowserEffectKind::RefreshCatalog, id, catalog->label,
            catalog->WorkingDatabase(), active});
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
    result.browserEffects.push_back({BrowserEffectKind::RemoveCatalog, id});
    session_.Remove(id);
    if (const auto* next = session_.ActiveCatalog()) {
        result.browserEffects.push_back({BrowserEffectKind::SelectCatalog, next->id});
    } else {
        result.browserEffects.push_back({BrowserEffectKind::Clear});
    }
    result.presentation.refreshBrowserStatus = true;
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::RequestWindowClose() {
    ControllerResult result;
    if (scans_.IsRunning()) {
        if (!closePending_) {
            closePending_ = true;
            scans_.RequestCancel();
        }
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
            scans_.RequestCancel();
            result.messages.push_back(Message(
                L"The active scan is being cancelled. Start the new scan after cancellation completes.",
                L"Cancelling scan", MB_OK | MB_ICONINFORMATION));
        } else {
            result.messages.push_back(Message(L"Scan cancellation is still pending.",
                L"Cancelling scan", MB_OK | MB_ICONINFORMATION));
        }
        PopulatePresentation(result);
        return result;
    }
    for (const auto* catalog : session_.OpenCatalogs()) {
        if (catalog->IsEditable()) {
            result.request.catalogChoices.push_back({catalog->id, catalog->label, catalog->path});
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

ControllerResult CatalogWorkflowController::MediaSelectionCompleted(
    const std::optional<wit::ui::AddNewDiskMediaResult>& media) {
    ControllerResult result;
    if (!media) {
        PopulatePresentation(result);
        return result;
    }
    auto* target = session_.Find(media->destinationCatalogId);
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
    if (!scans_.Start(target->WorkingDatabase(), *media, scanId)) {
        result.presentation.appStatus = AppStatus::Idle;
        result.messages.push_back(Message(L"Unable to prepare pending catalog changes.",
            L"Add/Update Disk Image", MB_OK | MB_ICONERROR));
    } else {
        activeScanId_ = scanId;
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
        }
    }
    PopulatePresentation(result);
    return result;
}

ControllerResult CatalogWorkflowController::OnScanProgress(ScanId scanId) {
    scans_.TakeProgress(scanId);
    ControllerResult result;
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
    result.presentation.canSave = active && active->IsEditable() && !scans_.Targets(active->id);
    result.presentation.canClose = active && !scans_.Targets(active->id);
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
