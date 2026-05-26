## Context

`MainFrame` now composes `MainWindowChrome`, `BrowserController`, `CatalogSession`, and `ScanCoordinator`, but it still contains the business workflow. It selects the active catalog, evaluates editability and scan targeting, decides Save/Discard/Close transitions, starts and adopts scans, chooses search eligibility, computes catalog-command eligibility, and owns `activeScanId_` and `closePending_`. The active `support-multiple-open-catalogs` and completed `safe-scan-threading-model` changes make those decisions catalog-identity-sensitive and lifetime-sensitive.

This change is constrained to an ownership refactor: native WTL/Win32 command routes and dialogs, `BrowserController` rendering/navigation, `MainWindowChrome` presentation, SQLite persistence behavior, staged pending databases, and asynchronous scan notification safety must continue to behave as specified.

## Goals / Non-Goals

**Goals:**
- Introduce a `CatalogWorkflowController` responsible for catalog lifecycle, pending-change, search, and media-scan business decisions.
- Make the controller the owner of `CatalogSession`, `ScanCoordinator`, expected scan identity, and close-during-cancellation state.
- Have `MainFrame` forward input and apply controller-provided UI effects/refresh data without inspecting domain state to choose behavior.
- Preserve current multi-catalog targeting, prompts, command availability, staged Save/Discard semantics, and safe scan completion adoption.

**Non-Goals:**
- Change user-visible workflows, wording, menu/accelerator IDs, dialog layouts, or command availability policy.
- Move native controls or browser navigation ownership into the workflow controller.
- Change `CatalogSession` persistence rules, `ScanCoordinator` threading model, SQLite schema, or settings format.
- Replace typed controller outcomes with a general event bus or new UI framework.

## Decisions

### 1. Add a catalog workflow controller above session and scan collaborators

Add `wit::app::CatalogWorkflowController` under `src/app`. It owns `CatalogSession`, `ScanCoordinator`, `activeScanId_`, and close/cancellation workflow state. It implements catalog activation, save/discard confirmation sequencing, close, Add/Update initiation and completion, and active-catalog search eligibility. Supporting logic currently coupled to those operations, such as active-catalog resolution, command availability calculation, startup activation, settings/recent-catalog access required by catalog commands, and scan notification interpretation, also moves behind its interface.

`BrowserController` and `MainWindowChrome` remain separate because they own control-facing state and presentation, not catalog policy.

Alternative considered: extend `CatalogSession` to contain all workflow behavior. Rejected because prompts, asynchronous operation coordination, and search/media intents are application workflow concerns rather than persisted catalog-session state.

### 2. Communicate through typed inputs and UI effects

The controller accepts intent inputs such as selected catalog identity, selected paths, accepted media-dialog data, prompt answers, and scan-notification ids. It returns a `ControllerResult` containing zero or more typed effects required to render or obtain further user input. Effects cover browser mutations, status and command-state snapshots, recent-menu models, error/information/prompt requests, media/search dialog requests, app busy state, and close-window authorization.

`MainFrame` may switch on effect type only to perform the specified UI operation and pass resulting user input back to the controller. It SHALL NOT reinterpret session state, validate a target, choose Save versus Discard, calculate enablement, or decide whether an operation is allowed.

Alternative considered: pass window/control handles into the controller and allow it to call dialogs and chrome methods directly. Rejected because that would conflate policy with native presentation and make the new class another UI host.

### 3. Treat UI-mediated workflows as resumable controller operations

Operations needing UI data are split into explicit controller steps. For example, a close intent can yield a close-confirmation request, then a pending-change request, then a browser removal/refresh result; Add/Update can yield a dialog request with editable destination choices, then validate the returned destination and begin a scan; search can either yield an informational message or a search-dialog request bound to the selected working database.

Prompt text and dialog invocation remain presentation effects so the frame can host modal native UI. The workflow decision that follows each answer remains in the controller. This ensures cancel, failed save, stale destination, and no-active-catalog branches cannot drift back into `MainFrame`.

Alternative considered: keep prompts in `MainFrame` because they are UI. Rejected for these workflows because the frame would still own the branching policy around prompt answers.

### 4. Route asynchronous scan lifecycle through the controller

The controller attaches/detaches its owned `ScanCoordinator` to the frame notification target and consumes `WM_SCAN_PROGRESS` and `WM_SCAN_COMPLETE` inputs forwarded by `MainFrame`. It validates the expected scan id, handles cancellation/close-pending state, transfers completed pending database ownership into its `CatalogSession`, and returns presentation/browser updates. `MainFrame` does not retain `ScanId`, call `TakeResult`, or inspect a completion outcome.

Alternative considered: leave scan message adoption in `MainFrame` because Win32 delivers the message there. Rejected because receiving a notification is UI plumbing, while accepting or discarding a result is the core destination/lifetime decision.

### 5. Render browser and chrome updates from controller snapshots

The controller does not own tree/list controls. Its results identify browser actions such as add, refresh, remove, select, or clear catalog, including required catalog ids, labels, and current working-database access; they also provide status/command availability and recent-menu data. `MainFrame` applies these effects to `BrowserController` and `MainWindowChrome` in declared order and reports selection changes back as new controller input.

This permits current view classes to remain intact while preventing refresh methods from recomputing eligibility through direct `CatalogSession` or `ScanCoordinator` access.

Alternative considered: move `BrowserController` under the new controller. Rejected because navigation/view ownership is already cohesive and should not be made dependent on catalog workflow policy.

## Risks / Trade-offs

- [Typed effects become an overly broad UI command language] -> Limit effects to existing workflow outputs and keep control-specific rendering inside `MainWindowChrome` and `BrowserController`.
- [A frame helper silently retains a business branch] -> Remove direct `CatalogSession`/`ScanCoordinator` members and includes from `MainFrame`, and review every remaining use of catalog state or scan status against the no-decision rule.
- [Resumable prompt workflows complicate close and shutdown handling] -> Model prompt continuation explicitly and verify close, exit, cancel, save failure, and scan-cancellation cases before removing existing logic.
- [Browser effects carry a database pointer whose catalog may later close] -> Apply each effect synchronously on the UI thread and continue resolving durable workflow identity by `CatalogId` within the controller.
- [Concurrent active changes shift current APIs during implementation] -> Integrate with the existing multi-catalog identity and generation-tagged scan behavior rather than redesigning either contract.

## Migration Plan

1. Add the controller result/effect types and `CatalogWorkflowController` ownership shell, transferring session/scan fields and read-only UI snapshot queries first.
2. Rewire initialization, active selection, catalog-command availability, scan notifications, and close/cancellation handling so `MainFrame` forwards inputs and renders returned effects.
3. Move `ActivateCatalog`, Save/Discard/Close confirmation flows, Add/Update, and Search workflows into resumable controller actions; remove their business helpers and state reads from `MainFrame`.
4. Add the new source files to the project and update architectural documentation/tests or verification notes enforcing that the frame has no catalog/scan decision ownership.
5. Build `Release|x64` and smoke test catalog startup/open/save/close, pending prompts, multi-catalog targeting, search, background scan completion/cancellation, status and command enablement, and application close.

Rollback is source-only: revert the delegation refactor and return ownership to the existing frame implementation. No stored catalog or settings migration is introduced.

## Open Questions

None block implementation. Exact effect type names may follow existing C++ naming conventions, provided the controller owns all decisions and the frame only performs requested presentation/refresh work.
