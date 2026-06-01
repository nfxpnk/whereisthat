#pragma once

#include <Windows.h>
#include <optional>
#include <string>
#include <vector>
#include <wit_catalog/CatalogSession.h>
#include <wit_gui/MainWindowChrome.h>
#include <wit_scanner/ScanCoordinator.h>
#include <wit_scanner/ScanRequest.h>

namespace wit::app {

enum class BrowserEffectKind {
    AddCatalog,
    RefreshCatalog,
    RemoveCatalog,
    SelectCatalog,
    Clear
};

struct BrowserEffect {
    BrowserEffectKind kind{BrowserEffectKind::Clear};
    wit::core::CatalogId catalogId{};
    std::wstring label;
    wit::storage::Database* database{};
    bool select{};
};

struct MessageEffect {
    std::wstring text;
    std::wstring title;
    UINT type{MB_OK};
};

struct CatalogChoice {
    wit::core::CatalogId id{};
    std::wstring label;
    std::wstring path;
};

enum class RequestKind {
    None,
    ChooseNewCatalog,
    ChooseOpenCatalog,
    ConfirmCloseCatalog,
    ConfirmPendingChanges,
    ShowSearch,
    ShowAddOrUpdateMedia,
    ShowGeneralSettings
};

struct RequestEffect {
    RequestKind kind{RequestKind::None};
    wit::storage::Database* database{};
    std::vector<CatalogChoice> catalogChoices;
    wit::core::CatalogId preferredCatalogId{};
    wit::platform::AppSettings settings;
};

struct PresentationEffect {
    std::wstring catalogStatus{L"No catalog"};
    bool protectedCatalog{};
    bool canScan{};
    bool canSave{};
    bool canClose{};
    bool refreshBrowserStatus{};
    bool refreshRecentMenu{};
    std::vector<std::wstring> recentCatalogPaths;
    bool updateStatusVisibility{};
    bool statusVisible{true};
    bool updateAppStatus{};
    AppStatus appStatus{AppStatus::Idle};
    bool flushStatus{};
};

enum class ScanDialogAction {
    None,
    Show,
    Update,
    Cancelling,
    Close
};

struct ScanDialogEffect {
    ScanDialogAction action{ScanDialogAction::None};
    std::uint64_t files{};
    std::uint64_t folders{};
    std::uint64_t totalFiles{};
    std::uint64_t remainingFiles{};
    bool totalKnown{};
    bool counting{};
};

struct ControllerResult {
    std::vector<BrowserEffect> browserEffects;
    std::vector<MessageEffect> messages;
    RequestEffect request;
    PresentationEffect presentation;
    ScanDialogEffect scanDialog;
    bool destroyWindow{};
};

class CatalogWorkflowController {
public:
    bool AttachTarget(HWND target);
    void DetachTarget();

    ControllerResult Initialize();
    ControllerResult SelectCatalog(wit::core::CatalogId id);
    ControllerResult RequestNewCatalog();
    ControllerResult RequestOpenCatalog();
    ControllerResult RequestOpenRecentCatalog(std::size_t index);
    ControllerResult CreateCatalogPathSelected(const std::optional<std::wstring>& path);
    ControllerResult OpenCatalogPathSelected(const std::optional<std::wstring>& path);
    ControllerResult RequestSave();
    ControllerResult RequestCloseCatalog();
    ControllerResult AnswerCloseCatalog(int answer);
    ControllerResult AnswerPendingChanges(int answer);
    ControllerResult RequestWindowClose();
    ControllerResult RequestSearch();
    ControllerResult SearchClosed();
    ControllerResult RequestAddOrUpdateMedia();
    ControllerResult MediaSelectionCompleted(const std::optional<wit::core::ScanRequest>& request);
    ControllerResult RequestCancelScan();
    ControllerResult RequestGeneralSettings();
    ControllerResult GeneralSettingsCompleted(const std::optional<wit::platform::AppSettings>& settings);
    ControllerResult OnScanProgress(ScanId scanId);
    ControllerResult OnScanComplete(ScanId scanId);

    wit::storage::Database* WorkingDatabase(wit::core::CatalogId id);
    std::wstring CatalogLabel(wit::core::CatalogId id) const;

private:
    enum class PendingContinuation {
        None,
        CloseCatalog,
        CloseWindow
    };

    CatalogSession session_;
    ScanCoordinator scans_;
    ScanId activeScanId_{};
    bool closePending_{};
    wit::core::CatalogId promptCatalogId_{};
    PendingContinuation pendingContinuation_{PendingContinuation::None};

    ControllerResult ActivateCatalog(const std::wstring& path, bool createNew, bool persistPath,
        const std::wstring& failureTitle, const std::wstring& failureMessage, UINT failureType);
    ControllerResult SaveCatalog(wit::core::CatalogId id);
    ControllerResult ContinuePendingWorkflow(wit::core::CatalogId id, PendingContinuation continuation);
    ControllerResult ContinueCloseCatalog(wit::core::CatalogId id);
    ControllerResult ContinueWindowClose();
    void DiscardPending(wit::core::CatalogId id, ControllerResult& result);
    void PopulatePresentation(ControllerResult& result, bool refreshRecentMenu = false);
    void Append(ControllerResult& destination, ControllerResult source);
    OpenCatalog* ActiveCatalog();
};

}
