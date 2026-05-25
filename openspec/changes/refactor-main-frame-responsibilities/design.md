## Context

`MainFrame` is a raw Win32 top-level window class whose implementation has grown to more than 1,100 lines. It registers and dispatches the frame window, creates the toolbar/status/tree/navigation/list controls, manages splitter layout and status drawing, coordinates tree/list navigation, owns active and pending databases plus recent/settings state, starts and joins the scan worker, receives worker messages, and implements most commands. Existing `src/ui` adapters help render the tree and owner-data list, but the orchestration and state for nearly every main-window feature still meet in the frame.

The application already vendors WTL 10.01 headers, initializes `WTL::CAppModule`, and uses ATL/WTL-style dialog message maps. Canonical specifications permit incremental WTL/ATL use. This change prepares an understandable ownership model before the follow-on conversion of the remaining frame and control surface to WTL.

## Goals / Non-Goals

**Goals:**
- Reduce `MainFrame` to top-level window lifetime, central event dispatch, command entry points, and composition of collaborating components.
- Divide current frame behavior into four focused collaborators with explicit ownership and communication boundaries.
- Preserve all current browser, menu/toolbar, status, settings, staged-save, protected-catalog, and asynchronous scan behavior.
- Amend canonical architecture and coding rules so future main-window functionality respects the extracted boundaries.
- Leave components in a shape that can be migrated individually to WTL in a later change.

**Non-Goals:**
- Convert `MainFrame`, child controls, or existing view adapters to WTL window/control wrappers in this change.
- Add new user commands, visual changes, storage semantics, threading policy, or database/schema changes.
- Redesign existing dialog classes or remove permitted direct Win32/Common Controls use.
- Enforce an arbitrary source-line limit when responsibilities are demonstrably cohesive.

## Decisions

- Retain `MainFrame` as the composition root and top-level event gateway. It owns creation/show/lifetime of the frame window, maps incoming frame events or commands to a collaborator, and coordinates interactions that cross collaborators. It does not retain implementations for browser history, catalog persistence state, scan-worker lifetime, or child-chrome rendering. Alternative considered: replace the frame in the same change with `WTL::CFrameWindowImpl`, rejected because mixing framework conversion with extraction makes regressions difficult to isolate.

- Introduce four collaborators, resulting in five main-window classes including `MainFrame`:
  - `MainWindowChrome` owns child control creation/handles, toolbar construction, splitter/layout behavior, status presentation/drawing, display-setting application, and control-level support such as the contents-list subclass. It exposes the handles/notifications required by orchestration without owning catalog or storage semantics.
  - `BrowserController` owns `CatalogTreeView`, `FileListView`, current `BrowserLocation`, history/index and tree-selection synchronization. It accepts the current readable database/session view from orchestration and supplies browser notification operations and focused/selected presentation data.
  - `CatalogSession` owns active database path, real and pending databases, dirty/protected state, settings/recent-file persistence, catalog activation/save/discard decisions, and access to the working readable database. It contains no window/control behavior.
  - `ScanCoordinator` owns worker-thread lifetime and scan-in-progress state, constructs staged scan work, and defines UI-thread completion/progress payload handling. It uses frame posting as the UI marshalling boundary and hands successful pending results to `CatalogSession` on the UI thread.
  
  Alternative considered: only extract toolbar/status and leave session/browser/scanning in the frame, rejected because most coupling and risk would remain centralized.

- Keep command routing in `MainFrame` only at the coordination level. Native menu, toolbar, accelerator, and context-menu IDs continue to reach the same top-level command entry point; the frame invokes `CatalogSession`, `BrowserController`, `ScanCoordinator`, or dialog presentation as appropriate and requests refreshes across collaborators after state changes. Alternative considered: create an additional command-controller class immediately, rejected because the four existing responsibility groups remove the present overload without adding a sixth owner for thin delegation.

- Separate collaborator communication through narrow calls and state snapshots rather than mutual ownership. `MainFrame` owns all four collaborators; `BrowserController` is rebound with `CatalogSession::WorkingDatabase()` when session state changes; `ScanCoordinator` returns/post-messages outcomes rather than directly modifying controls or catalog state; and chrome receives already-formatted/status state required to display. Alternative considered: pass `MainFrame*` into each extracted object for callbacks, rejected because it would make the prior coupling less visible rather than remove it.

- Treat this refactor as behavior-preserving. Existing `WM_SCAN_PROGRESS`/`WM_SCAN_COMPLETE` UI-thread marshalling, pending-database Save/Discard semantics, recent/last-catalog settings, owner-data paging, tree/list history synchronization, context menu and toolbar command IDs, status fields, and splitter layout remain observable acceptance criteria. Alternative considered: simplify or modernize flows during extraction, rejected because behavioral work should be proposed and validated independently.

- Add the long-lived rule to `docs/spec/architecture.md` and `docs/spec/coding-rules.md`: `MainFrame` is an orchestration/composition boundary, and new browser, chrome, catalog-session, or scan behavior must be implemented in its owning component or a justified focused component. The architecture specification will also clarify that current WTL 10.01 adoption is partial and that subsequent frame/control migration follows the decomposed boundaries. Alternative considered: document the rule only in this change, rejected because archived proposals do not serve as the project's canonical ongoing contract.

## Risks / Trade-offs

- [Extracted objects need coordinated refreshes after catalog activation, save, discard, or scan completion] -> Keep `MainFrame` responsible for explicit cross-component workflow ordering and cover each existing refresh trigger during verification.
- [Moving worker ownership can introduce shutdown or stale-session races] -> Preserve joining, scan-in-progress guards, UI-thread result acceptance, and active-session validation while relocating them to `ScanCoordinator`.
- [Moving control handles can break notification/subclass routing or owner-draw status updates] -> Keep native control IDs and dispatch routes stable, expose only required chrome handles, and smoke test controls, splitter, `Ctrl+A`, toolbar dropdown, and status drawing.
- [Four components can become facades that merely call back into a still-large frame] -> Move state and implementation with each responsibility and verify the frame header no longer owns corresponding state groups.
- [A later WTL migration may reshape these class APIs] -> Optimize now for coherent ownership and preserved behavior; permit the follow-on migration to adapt window-facing implementation without moving domain/session responsibilities back into the frame.

## Migration Plan

1. Update canonical architecture and coding rules with the thin-frame and collaborator ownership contract, including the explicitly subsequent WTL frame/control migration.
2. Add the four collaborator classes and project compilation entries, initially transferring state and helper behavior without changing externally visible operations.
3. Rewire `MainFrame` message and command dispatch to compose/delegate to the collaborators and remove transferred members and implementation.
4. Build `Release|x64` and smoke test startup/catalog lifecycle, browsing/navigation, toolbar/status/splitter behavior, settings/recent files, scan completion, pending Save/Discard, and exit protection.
5. Propose the separate WTL migration against the extracted window/control classes once this behavior-preserving baseline is verified.

Rollback consists of reverting the source/documentation refactor; no catalog files, settings format, or stored data require migration.

## Open Questions

None block the proposal. Class filenames and the smallest public interfaces may be adjusted while implementing if existing include or resource organization suggests clearer names, provided the four responsibility boundaries remain intact.
