## Context

The current main frame initializes `db_` from a hard-coded `catalog.db` path during window creation. `File > New Catalog` inserts an empty row into that database, and `Edit > Add/Update Disk Image` inserts another row and scans into it. The schema therefore models the `catalogs` rows as scanned media sets, even though the product term "catalog" must now mean the SQLite file itself.

The app already has a separate native `settings.ini` helper for `General.ShowStatusBar`, and storage/scanning are split between `src/storage` and `src/core`. This change must preserve native Win32/C++20 implementation, SQLite C API deployment, background scanning, paging, and existing user data, including user database files named `catalog.db`.

## Goals / Non-Goals

**Goals:**
- Make a selected SQLite file the active catalog and support an empty state with no active catalog.
- Create and open catalog files through native dialogs and make switches visible in loaded data and application state.
- Persist and restore only the last successfully activated catalog path.
- Make repeated Add/Update operations replace or refresh the selected media source within the active catalog rather than accumulating duplicate source contents.
- Preserve compatibility with existing database files that already contain the current schema and data.

**Non-Goals:**
- Automatically import, merge, split, or rename existing `catalog.db` data.
- Automatically select an existing `catalog.db` on a machine that has no stored last-used path.
- Implement Save, Save As, Close, Catalog Manager, or other placeholder menu behavior.
- Redesign file-list paging, add a managed UI layer, or change SQLite packaging.

## Decisions

- Represent active-catalog state as an optional opened database path and usable `Database` connection owned by main-frame orchestration. Startup loads settings before attempting a database open; views remain cleared and catalog-dependent commands are unavailable or report that a catalog must be created/opened when no activation succeeds. Alternative considered: continue opening `catalog.db` implicitly, rejected because it violates empty startup and makes the filename special.
- Keep the existing schema readable as a catalog-file schema, treating current `catalogs` rows as indexed media-source records within one catalog database for compatibility. The implementation can clarify UI naming where necessary without destructively transforming existing files. Alternative considered: immediately replace the schema with one file-wide metadata row, rejected because it would add migration risk not needed to establish independent catalog files.
- Implement activation as one operation shared by startup restoration, Open, and successful New: open/initialize a candidate database, replace the current connection/path only after success, reload views from that connection, and persist `General.LastCatalogPath` only for user-created/opened or successfully restored active catalogs. Alternative considered: assign the current path before validating the new database, rejected because failure could discard a functioning catalog or persist an unusable path.
- Use a native save-file dialog for New Catalog and a native open-file dialog for Open, filtering for SQLite/database files while allowing ordinary filesystem paths. New Catalog must target a path that does not already exist, initialize a fresh empty schema, then activate it; selecting an existing file must not overwrite it and directs the user to Open instead. Alternative considered: allow overwrite confirmation, rejected because "brand-new" and "must never modify the old/current catalog" require stronger protection.
- Extend `AppSettings` in `src/platform` with the last-used catalog path in `settings.ini`, alongside but independent of display preferences. An absent or empty key means there is no startup catalog; an unreadable, missing, or invalid saved database leaves the UI empty without creating a replacement database. Alternative considered: store recent path state in SQLite, rejected because no database may be active at startup and settings must remain outside catalog content.
- Scope Add/Update Disk Image to an active database and identify a scanned media source by its normalized selected root/source path in that database. For an existing source, refresh its stored files and aggregate count without creating a second source entry; for a new source, add it and scan it. The refresh should use storage-layer transaction behavior so a failed scan does not leave a duplicated partial replacement. Alternative considered: preserve the current append-on-every-scan behavior, rejected because it creates duplicates and does not express update semantics.
- Prevent catalog switching and New/Open activation while a background scan is active, reusing the existing scan-in-progress guard. The worker receives the currently active database path captured at scan start and must not mutate any previously or subsequently activated file. Alternative considered: switch while scanning into an older connection, rejected because it obscures which catalog is being modified and complicates view consistency.
- Update canonical documentation to say that `catalog.db` is merely a valid filename for one catalog database, not an implicit default. Alternative considered: leave documentation as historical description, rejected because it would contradict the new storage and startup requirements.

## Risks / Trade-offs

- Existing databases contain multiple rows historically called catalogs, while the file becomes the product catalog -> retain the schema as compatible media-source records and update terminology/documentation before pursuing any later schema cleanup.
- A stored path can reference deleted, inaccessible, or corrupt user data -> attempt a non-destructive open at startup and fall back to a clear empty state without creating or overwriting a file.
- Refreshing a source could erase useful data if a rescan fails -> provide transactional replacement/update behavior and commit only successful scan results.
- Switching database connections while list views or a worker still reference the old connection can cause invalid access or cross-file writes -> centralize activation, clear/rebind views, and prohibit switching during scans.
- Strict New-file non-overwrite behavior adds one user correction step if a filename already exists -> protect existing catalogs and direct existing-file use through Open.

## Migration Plan

1. Extend canonical product, architecture, storage, UI, and acceptance specifications from fixed `catalog.db` to selected catalog database files and empty startup state.
2. Extend settings persistence with an optional last-used catalog path while preserving the current default for `ShowStatusBar`.
3. Refactor active database ownership and view binding so no database is required until activation and an activation failure leaves current or empty state intact.
4. Implement New/Open dialogs and activation persistence; remove implicit startup opening of `catalog.db`.
5. Implement active-catalog Add/Update source refresh behavior using transactional storage operations and validate cross-file isolation.

No eager data migration is required: an existing database file remains usable when opened as a catalog. Rollback of the application code does not delete any catalog file; users who created catalogs under the new behavior would need to explicitly select the desired file in a version that still assumes one configured database.

## Open Questions

- No decision blocks implementation. A later change may rename the existing source-list/domain types and schema tables once the catalog-file model is established without risking stored user data.
