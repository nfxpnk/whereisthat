## 1. Catalog Contract And Documentation

- [x] 1.1 Update `docs/spec/product.md`, `docs/spec/storage.md`, and `docs/spec/architecture.md` so a catalog is a user-selected SQLite database file and `catalog.db` is only a possible filename, preserving compatibility expectations for existing files.
- [x] 1.2 Update `docs/spec/ui.md` and `docs/spec/acceptance.md` for New/Open active-database switching, empty startup behavior, last-used restoration, and active-catalog-only Add/Update behavior.

## 2. Settings And Active Database State

- [x] 2.1 Extend the native `AppSettings` model and `settings.ini` persistence helper with an optional `General.LastCatalogPath` value while preserving the existing `ShowStatusBar` default and save behavior.
- [x] 2.2 Refactor the main-frame database/view state so it can represent no active catalog, activate a validated database path atomically, reload or clear bound views on activation changes, and avoid an implicit `catalog.db` open.
- [x] 2.3 Add storage connection lifecycle/error handling needed to test and switch candidate SQLite database files without corrupting the current active connection on failure.

## 3. Catalog Activation Workflows

- [x] 3.1 Implement startup restoration that activates the saved last-used catalog only when it remains available and usable, otherwise leaving the application in the empty state without creating a database.
- [x] 3.2 Replace the New Catalog row-creation dialog behavior with a native save-path workflow that creates an initialized empty SQLite database, activates it, and stores its path without mutating the formerly active catalog.
- [x] 3.5 Permit native-confirmed replacement of a closed existing New Catalog destination while refusing a destination already open in the application with a specific conflict message.
- [x] 3.3 Implement `File > Open` with a native existing-database selection workflow that activates and stores a successfully opened catalog while retaining the prior active state after failures.
- [x] 3.4 Route the empty-state user experience so New/Open remain usable and catalog-dependent Add/Update reports that a catalog must first be created or opened.

## 4. Active-Catalog Media Updates

- [x] 4.1 Add storage-layer lookup and transactional replacement operations that identify an indexed media source by normalized selected path within the active database and refresh its file rows/item count without duplicate source records.
- [x] 4.2 Change `Edit > Add/Update Disk Image` and background scan orchestration to require an active catalog, capture only its active path for worker writes, add new sources or refresh existing sources, and block catalog switching while scanning.
- [x] 4.3 Rebind/reload displayed source and file data after a successful scan so updated contents are shown from the active catalog only.

## 5. Verification

- [x] 5.1 Build `whereisthat.sln` for `Release|x64` with MSBuild and confirm the existing native Win32/SQLite DLL deployment remains intact.
- [x] 5.2 Smoke test empty startup, New/Open switching between differently named database files, persistence/restoration of the last-used path, and missing/unusable saved-path fallback.
- [x] 5.3 Smoke test that New Catalog does not modify any open or previously active file and that repeated Add/Update refreshes one source only in the active catalog while another database remains unchanged.
- [ ] 5.4 Smoke test native-confirmed replacement of a closed existing New Catalog destination and the open-catalog conflict message.

## 6. Recent Catalog Access And Settings Display

- [x] 6.1 Extend `AppSettings` to persist a deduplicated most-recently-used list of up to ten catalog paths separately from SQLite content.
- [x] 6.2 Display the stored last-opened catalog path read-only in General Settings.
- [x] 6.3 Implement `File > Open Recent` population and validated activation for recent catalog entries.
- [x] 6.4 Build `whereisthat.sln` for `Release|x64` after the recent-catalog and General Settings additions.
- [ ] 6.5 Smoke test recent-list ordering, ten-entry trimming, successful recent activation, unavailable-entry handling, and General Settings display.
