## Context

The application is a native C++20 Windows desktop program and already compiles vendored WTL 10.01 with installed ATL. `main.cpp` initializes `WTL::CAppModule`, while `AddNewDiskMediaDialog` and `SearchDialog` use ATL dialog message maps. After `refactor-main-frame-responsibilities`, the main surface is split into `MainFrame` as coordinator, `MainWindowChrome` for child presentation, `BrowserController` for navigation, `CatalogSession` for state/persistence, and `ScanCoordinator` for asynchronous work.

The remaining raw infrastructure is concentrated where migration is appropriate: `MainFrame` manually registers a window class and routes messages through `GWLP_USERDATA`, `App` runs a hand-written accelerator loop, and `MainWindowChrome` uses raw control handles and a list subclass procedure. The migration must stay native and must not perturb catalog behavior or reverse the decomposition.

## Goals / Non-Goals

**Goals:**
- Host the primary frame through WTL 10.01 window/frame infrastructure and message maps.
- Use the WTL message loop and accelerator/filter path consistently with existing `_Module` lifetime.
- Adopt WTL/ATL control/window wrappers in chrome or adapters where they replace hand-managed window plumbing cleanly.
- Retain the extracted responsibility boundaries and all observable workflows.
- Update documentation and verification for completed WTL main-window adoption.

**Non-Goals:**
- Redesign catalog session, browser navigation, scan workflow, SQLite schema, settings file, menus, or visual layout.
- Replace Common Controls, native resources, or Win32 messaging with a managed or cross-platform UI system.
- Convert every direct Win32 call when a native API call remains clearer for drawing, shell dialogs, or background message posting.
- Add a WTL runtime binary or new external dependency.

## Decisions

- Derive `MainFrame` from `WTL::CFrameWindowImpl<MainFrame>` and use a WTL frame class declaration/message map for create, size, close/destroy, command, notify, draw, splitter mouse, and application-defined scan messages. `MainFrame` continues to compose and coordinate the same four collaborators; inheriting WTL window hosting does not grant it their responsibilities. Alternative considered: use a minimal `ATL::CWindowImpl` only, rejected because `CFrameWindowImpl` supplies the supported native frame/accelerator integration expected for the application's top-level window.

- Change `App::Run()` to register a `WTL::CMessageLoop` with `_Module`, attach the frame as an appropriate message filter or route its `PreTranslateMessage` for accelerators, run the loop, and remove it during shutdown. Preserve the existing accelerator resource and command IDs. Alternative considered: retain the hand-written `GetMessage` loop after converting the frame, rejected because it leaves the existing WTL application-module infrastructure half-used at the primary lifetime boundary.

- Keep `MainWindowChrome` as a collaborator and adopt wrapper members such as `ATL::CWindow` or relevant WTL common-control wrappers for owned child HWNDs only where this improves creation/message calls and notification support. Owner-draw status painting, WIC bitmap loading, and narrowly suitable raw native operations may remain direct Win32 code. Alternative considered: fold chrome into `CFrameWindowImpl` convenience members, rejected because that reintroduces presentation/layout ownership into the coordinating frame.

- Keep `BrowserController`, `CatalogSession`, and `ScanCoordinator` independent of WTL frame inheritance. `BrowserController` may receive wrapped control handles or continue to use HWND-compatible view adapters; `CatalogSession` remains UI-free; `ScanCoordinator` continues posting application messages to the frame HWND for UI-thread acceptance. Alternative considered: route worker completion through direct component callbacks, rejected because the current message marshalling is native, safe, and already behavioral contract.

- Preserve existing resource-defined menus, accelerators, toolbar/control IDs, status sections, splitter semantics, and native dialogs. The WTL migration is validated as an infrastructure replacement with identical functional routes. Alternative considered: adopt WTL rebar/command-bar presentation simultaneously, rejected because it would introduce visible toolbar/menu changes not requested by the migration.

- Update canonical documents when implementation completes to state that main-frame hosting/message-loop/control plumbing uses WTL 10.01 while remaining a native Common Controls application. Alternative considered: rely on source alone, rejected because WTL adoption is an architectural and build-contract fact.

## Risks / Trade-offs

- [WTL frame defaults can add rebar/client-view behavior or alter native sizing] -> Use only the hosting/message-map features needed and retain `MainWindowChrome` layout/control creation and existing resource menu behavior.
- [Accelerators or custom scan messages stop reaching the existing coordination paths] -> Preserve command IDs and application message constants, wire message-map handlers explicitly, and test accelerator and scan completion routes.
- [Changing HWND wrappers or subclass ownership disturbs owner-data notifications or `Ctrl+A`] -> Migrate chrome/control handling incrementally and verify TreeView/ListView notifications, dropdowns, status drawing, and list keyboard handling.
- [The migration invites logic to move back into frame handlers] -> Keep handlers as delegations to the four existing collaborators and review the frame against the thin-frame architecture rule.

## Migration Plan

1. Update WTL support includes and introduce the WTL message-loop/accelerator lifetime in `App`.
2. Convert `MainFrame` hosting and dispatch to `CFrameWindowImpl` message maps while leaving collaborator APIs and workflow order intact.
3. Migrate suitable `MainWindowChrome` or window-facing adapter handle plumbing to WTL/ATL control wrappers without changing resources or layout.
4. Update canonical WTL documentation and build/acceptance language.
5. Build `Release|x64` and smoke test startup, accelerators, controls/layout, browsing, settings, status, asynchronous scanning, pending-save behavior, and shutdown prompts.

Rollback is a source-level reversion to the raw-window hosting implementation. The migration changes no on-disk catalog or settings data.

## Open Questions

None block implementation. The implementation can select the narrowest useful set of child-control wrappers after `CFrameWindowImpl` is compiled, as long as raw plumbing is not needlessly replaced and the collaborator boundaries remain enforced.
