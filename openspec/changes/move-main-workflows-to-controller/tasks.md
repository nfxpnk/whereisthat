## 1. Controller Contract And Ownership

- [x] 1.1 Add `CatalogWorkflowController` and typed result/effect/input definitions under `src/app`, and register its source/header in `WhereIsThat.vcxproj`.
- [x] 1.2 Move ownership of `CatalogSession`, `ScanCoordinator`, expected scan identity, and close/cancellation workflow state from `MainFrame` into the controller.
- [x] 1.3 Define controller-produced snapshots/effects for browser mutations, recent-menu data, status/command availability, app busy state, informational/prompt UI, dialog requests, and window-close authorization.

## 2. Frame Delegation And Rendering

- [x] 2.1 Rewire startup, catalog selection changes, command dispatch, and shutdown input so `MainFrame` forwards intents to the controller and mechanically applies returned effects to `BrowserController` and `MainWindowChrome`.
- [x] 2.2 Replace frame-side status, recent-menu, and command-availability decisions with controller-provided values while retaining existing native menu, toolbar, status-bar, and browser rendering routes.
- [x] 2.3 Route controller requests for file selection, media/search dialogs, message boxes, and prompt responses through `MainFrame` as UI operations whose returned input is sent back to the controller for the next decision.

## 3. Catalog And Search Workflows

- [x] 3.1 Move catalog activation logic, including startup restore and New/Open/Open Recent result handling, into controller operations that emit browser and recent/status refresh effects.
- [x] 3.2 Move `OnSaveCatalog`, discard, `ConfirmPendingChanges`, all-pending shutdown resolution, and `CloseCatalog` behavior into resumable controller operations that preserve per-catalog Save/Discard/Cancel and close-confirmation outcomes.
- [x] 3.3 Move `OnSearchForItems` eligibility and search-launch workflow into the controller, with the frame only displaying the requested no-catalog message or search dialog and rendering app-status effects.

## 4. Media Scan Workflow And Completion

- [x] 4.1 Move `OnAddOrUpdateDiskImage` policy into the controller, preserving editable-destination selection, cancellation behavior, stale destination validation, scan start failure reporting, and busy/command-state output.
- [x] 4.2 Forward scan progress/completion messages to the controller and move generation validation, worker retirement, cancellation/close continuation, and pending database adoption out of `MainFrame`.
- [x] 4.3 Verify completed scans refresh the correct destination catalog and that invalid, stale, cancelled, or failed results do not update catalog state through frame-level decisions.

## 5. Thin-Shell Enforcement

- [x] 5.1 Remove the moved workflow methods, `CatalogSession`/`ScanCoordinator` ownership, scan workflow state, and direct catalog/scan policy access from `MainFrame`.
- [x] 5.2 Audit remaining `MainFrame` event handlers and helpers so conditionals select only UI rendering/dispatch by controller effect type, never catalog eligibility, persistence, pending-change, scan-target, or search decisions.
- [x] 5.3 Update the canonical architecture/coding rule documentation or equivalent project contract to state that main-window business decisions belong to `CatalogWorkflowController` and that `MainFrame` only delegates and renders outcomes.

## 6. Verification

- [x] 6.1 Build `Release|x64` through the supported solution and fix compilation or linkage regressions introduced by the new controller files and delegation API.
- [ ] 6.2 Smoke test startup and recent-catalog restore, create/open, multi-catalog selection, Save, Close, pending Save/Discard/Cancel, search, Add/Update, scan completion/cancellation, protected catalogs, and application close.
- [x] 6.3 Review `MainFrame.h` and `MainFrame.cpp` against the `main-workflow-controller` specification and confirm no decision-making business workflow remains in the shell.
