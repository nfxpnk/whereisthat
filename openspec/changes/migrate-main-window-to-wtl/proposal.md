## Why

The application already vendors WTL 10.01 and uses it for module lifetime and dialogs, but its main frame and child-control plumbing still manually implement registration, dispatch, subclassing, and portions of native UI management. Now that `MainFrame` has been decomposed into focused collaborators, those window-facing pieces can be migrated without carrying forward a monolithic ownership design.

## What Changes

- Convert the top-level `MainFrame` window host and message routing from manual class registration/window-procedure plumbing to appropriate WTL 10.01 frame/window implementation and message-map patterns.
- Migrate window-facing operations in `MainWindowChrome` and suitable native control adapters to WTL/ATL wrapper types where they clarify control ownership and notification routing.
- Adopt the WTL application message-loop integration already supported by `WTL::CAppModule`, including existing accelerator handling through the native WTL loop/filter mechanisms.
- Preserve the `MainFrame`, `MainWindowChrome`, `BrowserController`, `CatalogSession`, and `ScanCoordinator` ownership split; WTL conversion must not move session, browser, or scanning logic back into the frame.
- Preserve native Windows appearance, resource IDs, menu/toolbar routes, TreeView/ListView behavior, status rendering, scan message marshalling, settings, and persisted catalog semantics.

## Capabilities

### New Capabilities

- `wtl-main-window-infrastructure`: Defines WTL 10.01-based top-level window and message-loop hosting while preserving native behavior and the decomposed main-window responsibility boundaries.

### Modified Capabilities

None. The migration changes native UI infrastructure without changing existing user workflow requirements.

## Impact

- `src/app/MainFrame.*`, `src/app/MainWindowChrome.*`, `src/app/App.*`, `src/app/WtlSupport.h`, and potentially window-facing adapters in `src/ui` will adopt WTL/ATL window/control support.
- Existing `third_party/wtl/Include` and the installed ATL MSVC component remain the only WTL/ATL prerequisites; no deployed runtime DLL or new dependency is added.
- `docs/spec/architecture.md`, `docs/spec/coding-rules.md`, and applicable build/UI documentation will record completed main-window WTL adoption.
- Catalog storage format, SQLite deployment, scanning behavior, and pending-save semantics remain unchanged.
