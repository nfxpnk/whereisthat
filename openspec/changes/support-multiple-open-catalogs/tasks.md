## 1. Contract And Session Model

- [x] 1.1 Update canonical architecture, UI, product, and acceptance documentation for a multi-open catalog workspace, active-by-TreeView selection, targeted Close, and Add New Disk/Media destination selection.
- [x] 1.2 Introduce a stable `CatalogId` and per-open-catalog state object in `CatalogSession` containing path/label, persisted and pending databases, editable/dirty state, and working-database access while keeping settings/recent-path ownership at workspace scope.
- [x] 1.3 Replace singleton activation/save/discard APIs with add-or-focus open/create, duplicate normalized-path detection, active lookup/set, per-catalog save/discard/remove, and open-catalog enumeration APIs.
- [x] 1.4 Preserve startup restore and recent-path behavior by opening the stored last-used catalog as the initial workspace item without requiring persistence of all concurrently open catalogs.

## 2. Catalog-Aware Browser Tree

- [x] 2.1 Add a browser target/node identity carrying `CatalogId` plus `BrowserLocation`, and refactor `CatalogTreeView` to insert, refresh, remove, classify, and expand multiple catalog roots using the appropriate working database.
- [x] 2.2 Refactor `BrowserController` navigation, history, address display, list binding, and tree notifications to resolve the selected catalog's working database and remove stale navigation entries when a catalog closes.
- [x] 2.3 Change New, Open, and Open Recent coordination in `MainFrame` to add or focus catalog roots rather than replace an existing session, and refresh selected-catalog status and command state after selection changes.
- [ ] 2.4 Verify two databases can be opened, independently expanded/browsed, and re-selected without cross-catalog contents or duplicate roots for equivalent paths.

## 3. Targeted Close And Pending Changes

- [x] 3.1 Route `ID_WIT_FILE_CLOSE` through active-catalog resolution so File > Close targets the selected root or the owning root of a selected descendant and is disabled when the workspace is empty.
- [x] 3.2 Update the TreeView popup to offer `Close Catalog` only for a catalog top-level item (or disable it for descendants) and dispatch it through the shared close action.
- [x] 3.3 Implement `CanCloseCatalog`/`CloseCatalog` coordination with the exact close confirmation text, `No` default, and per-catalog `Save changes?` Yes/No/Cancel resolution; keep a catalog open when Cancel or Save failure occurs.
- [x] 3.4 Remove only the confirmed catalog's tree, browser, database, and pending state; select a remaining fallback root or clear panes and disable catalog commands after closing the last root.
- [x] 3.5 Extend application shutdown handling to resolve pending changes for every modified open catalog and verify one catalog's save/discard/close never alters another catalog's state.

## 4. Destination Catalog In Media Dialog

- [x] 4.1 Add a resource ID and native combo-box row immediately after `Disk name:` in `IDD_ADD_NEW_DISK_MEDIA`, adjusting the remaining fixed dialog layout and tab order.
- [x] 4.2 Extend `AddNewDiskMediaDialog` inputs/results with value-based editable catalog choices and a destination `CatalogId`, preselecting the active editable catalog and requiring a valid destination before confirmation.
- [x] 4.3 Update `MainFrame::OnAddOrUpdateDiskImage()` to populate destination choices, revalidate the returned target, and start a scan from that target catalog's working database rather than implicit active singleton state.
- [ ] 4.4 Verify the dialog can explicitly target either of two editable catalogs and does not permit a protected/no-longer-valid destination.

## 5. Asynchronous Target Lifetime And UI State

- [x] 5.1 Carry destination `CatalogId` through `ScanCoordinator::Start`, active scan state, and `ScanResult`, and adopt completed pending data only into the matching open catalog on the UI thread.
- [x] 5.2 Guard save/discard/close of the scan destination while its worker is running while allowing browsing or closing unrelated catalogs, and refresh only affected roots/views/status on completion.
- [x] 5.3 Update menu and toolbar enablement/status rendering for the selected catalog, empty workspace, editable destinations, pending changes, and scan activity.

## 6. Verification

- [x] 6.1 Build `whereisthat.sln` for `Release|x64` and correct any compiler/resource regressions from the session, tree, dialog, and scan API changes.
- [ ] 6.2 Smoke test clean and modified catalog Close from a root context menu and File menu child selection, including safe-default No, Cancel, discard, successful Save, and failed/protected Save behavior.
- [ ] 6.3 Smoke test multiple-open browsing, duplicate-path focus, close fallback/empty state, startup restore, application exit with multiple dirty catalogs, and scan completion after selection moves to another catalog.
- [ ] 6.4 Smoke test Add New Disk/Media destination selection for two editable catalogs and confirm staged/save contents are written only to the chosen catalog database.
