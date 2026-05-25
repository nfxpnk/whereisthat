## Why

The application now treats a SQLite file as a catalog, but its runtime still owns and displays only one open catalog at a time. Users need to browse several catalog databases side by side and close or edit the intended catalog without replacing unrelated open work.

## What Changes

- **BREAKING** Replace the singleton active-catalog workspace with a multi-catalog workspace in which each opened database file remains independently open and appears as one top-level TreeView item.
- Make New, Open, and Open Recent add or focus a catalog in the workspace rather than replacing the currently displayed database, while avoiding duplicate open sessions for the same file path.
- Make selection of any tree item establish its owning catalog as the active command/browse target, with child disk/folder navigation still querying only that catalog's working view.
- Implement `File > Close` for the active catalog and expose `Close Catalog` from the TreeView context menu only for a catalog root item.
- Confirm a catalog close with a safe-default `No` choice, then resolve that catalog's pending changes with Save/Discard/Cancel behavior before removing only its tree root and database state.
- Keep pending edits, protection state, and save/discard transitions per opened catalog so modified work in one database is not lost or committed while operating on another.
- Add a catalog selector immediately after `Disk name:` in the Add New Disk/Media dialog; an accepted scan targets the selected editable open catalog, with the active catalog preselected.
- Update command enabled state, empty-workspace behavior, startup restoration, shutdown guarding, and scan-result ownership for a collection of open catalogs.

## Capabilities

### New Capabilities
- `multi-catalog-workspace`: Multiple concurrently opened catalog databases, catalog-aware browsing and activation, targeted Close commands, close confirmation, and per-catalog unsaved-change handling.
- `catalog-targeted-media-scan`: Selection of an open destination catalog in Add New Disk/Media and safe delivery of staged scan results to that catalog.

### Modified Capabilities

None promoted under `openspec/specs/`; this change extends the implemented lifecycle, TreeView menu, deferred-save, and media-dialog behavior defined by earlier in-flight changes.

## Impact

- Refactors `src/app/CatalogSession.*` from one active database/pending copy to a collection of catalog state objects and an active catalog identity; updates `MainFrame.*` command, prompt, startup, close, status, and scan coordination.
- Refactors `src/app/BrowserController.*`, `src/ui/CatalogTreeView.*`, `src/ui/FileListView.*`, and possibly `src/core/BrowserLocation.h` so tree/list/history operations resolve the owning open catalog without mixing database handles.
- Extends `src/ui/AddNewDiskMediaDialog.*` and `src/app/app.rc`/`resource.h` with destination-catalog selection and updates `ScanCoordinator.*` completion correlation.
- Preserves the SQLite catalog format and existing per-catalog staged Save model; no database migration or new external dependency is required.
