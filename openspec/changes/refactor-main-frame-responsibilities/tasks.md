## 1. Architecture Contract And Baseline

- [x] 1.1 Update `docs/spec/architecture.md` and `docs/spec/coding-rules.md` to require `MainFrame` to remain a composition/coordination boundary and assign chrome, browser, session, and scanning behavior to focused components.
- [x] 1.2 Record in the canonical architecture documentation that WTL 10.01 is already available and partially adopted, while migration of remaining main-frame/control code is a separate follow-on change after this extraction.
- [x] 1.3 Capture the current main-window behavior and a successful `Release|x64` build baseline for catalog lifecycle, browser navigation, status/toolbar/splitter interactions, settings/recent catalogs, scanning, pending Save/Discard, and close protection.

## 2. Catalog Session Extraction

- [x] 2.1 Add a `CatalogSession` class owning active catalog path, persisted and pending databases, dirty/protected state, settings/recent-file lifecycle, and working-database access.
- [x] 2.2 Move catalog activation, recent-menu model updates, Save/Discard, and pending-change decision logic out of `MainFrame` while retaining the existing state transitions and errors.
- [x] 2.3 Rewire catalog command workflows through `MainFrame` coordination to `CatalogSession` and verify browser/status refreshes still occur after activation, save, and discard.

## 3. Browser Coordination Extraction

- [x] 3.1 Add a `BrowserController` class owning `CatalogTreeView`, `FileListView`, current location, Back/Forward history, and selection synchronization.
- [x] 3.2 Move browser reload/clear/navigation operations and tree/list notification behavior out of `MainFrame`, binding reads through the current `CatalogSession` working database.
- [x] 3.3 Verify catalog-root disk display, folder activation, tree expansion/selection, Back/Forward behavior, owner-data paging, `Ctrl+A`, and focused/selected status inputs are preserved.

## 4. Main Window Chrome Extraction

- [x] 4.1 Add a `MainWindowChrome` class owning child-control creation/handles, toolbar setup, splitter/layout calculations, settings-driven status visibility, and status-bar presentation/drawing.
- [x] 4.2 Move control-facing notification/subclass support and toolbar/status operations from `MainFrame` while keeping native control IDs, command IDs, and UI appearance unchanged.
- [x] 4.3 Verify resizing, splitter dragging, toolbar buttons/dropdown, content-list keyboard handling, status fields/lights, and display-setting application remain functional.

## 5. Scan Coordination Extraction

- [x] 5.1 Add a `ScanCoordinator` class owning worker lifetime, in-progress state, staged scan startup, and progress/completion payload ownership.
- [x] 5.2 Retain Win32 message delivery as the UI-thread marshalling boundary and have `MainFrame` coordinate accepted completion results into `CatalogSession`, browser reload, and status refresh.
- [x] 5.3 Verify scan-in-progress guarding, non-blocking UI progress/completion behavior, pending staging, Save/Discard, and safe close/shutdown behavior.

## 6. Thin Frame Integration

- [x] 6.1 Reduce `MainFrame` members and implementations to top-level window lifetime, command/message dispatch, dialog launch entry points, and explicit cross-component refresh sequencing.
- [x] 6.2 Add new component source/header files to `WhereIsThat.vcxproj`, clean up transferred includes/helpers, and ensure no extracted state or logic remains duplicated in `MainFrame`.
- [x] 6.3 Review new main-window work against the `main-frame-decomposition` spec so future extension points and collaborator interfaces keep the frame thin.

## 7. Verification And Follow-On

- [x] 7.1 Build the application in `Release|x64` through the supported MSBuild solution and resolve extraction-related compile/link regressions.
- [ ] 7.2 Smoke test startup and last-used catalog behavior, create/open/recent workflows, native browser navigation, commands/toolbar/status/splitter, search/settings dialogs, background scanning, pending Save/Discard, protected catalogs, and exit prompts.
- [x] 7.3 Create the subsequent WTL main-frame/control migration proposal against the verified extracted classes, without moving browser, catalog-session, or scanning responsibilities back into `MainFrame`.
