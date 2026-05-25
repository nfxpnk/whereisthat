## 1. WTL Frame And Loop Foundation

- [x] 1.1 Extend `WtlSupport.h` with the WTL frame/control includes required by the migrated main-window implementation while retaining the existing `WTL::CAppModule` lifetime.
- [x] 1.2 Replace the hand-written `App::Run()` message loop with a registered `WTL::CMessageLoop` path that retains the existing accelerator resource behavior.
- [x] 1.3 Update `MainFrame` to use WTL 10.01 frame hosting and a native message map for create, resize, commands, notifications, owner drawing, splitter input, close/destroy, and scan messages.

## 2. Window-Facing Collaborator Migration

- [x] 2.1 Adopt appropriate WTL/ATL window or Common Controls wrappers in `MainWindowChrome` for owned child controls and toolbar/status interaction without moving layout ownership into `MainFrame`.
- [x] 2.2 Replace or adapt raw contents-list subclass plumbing through a WTL-compatible control/message mechanism while retaining `Ctrl+A` behavior and owner-data list presentation.
- [x] 2.3 Keep `BrowserController`, `CatalogSession`, and `ScanCoordinator` ownership and public workflow contracts intact while adapting only HWND/message surfaces required by WTL hosting.

## 3. Routing And Behavior Preservation

- [x] 3.1 Preserve menu, toolbar, accelerator, context-menu and placeholder command IDs and verify their existing routes reach the same coordinator operations under WTL dispatch.
- [x] 3.2 Preserve tree/list notifications, Back/Forward synchronization, status owner drawing, display settings, splitter dragging, and resource-defined native visual behavior.
- [x] 3.3 Preserve `WM_APP` scan progress/completion marshalling, pending database installation, Save/Discard behavior, protected-catalog handling, and shutdown guarding.

## 4. Documentation And Verification

- [x] 4.1 Update `docs/spec/architecture.md`, `docs/spec/coding-rules.md`, `docs/spec/build.md`, `docs/spec/ui.md`, and `docs/spec/acceptance.md` to record completed WTL 10.01 main-frame/message-loop adoption and continued collaborator boundaries.
- [x] 4.2 Build `whereisthat.sln` in `Release|x64` through MSBuild and confirm no new deployed WTL runtime or dependency is introduced.
- [ ] 4.3 Smoke test startup, accelerator handling, controls/layout/status presentation, catalog navigation, dialogs/settings, background scanning and staged Save/Discard/close workflows under WTL-hosted main-window routing.
