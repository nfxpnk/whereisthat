# Architecture

## Stack choice
Where Is That? uses C++20 + the Win32 API + WTL/ATL + SQLite + MSVC. The UI uses native Win32/Common Controls, with WTL/ATL wrappers introduced incrementally for windows and dialogs; storage goes through the SQLite C API, and builds are driven by the MSVC v143 toolset with MSBuild. This keeps the app small and native.

## Why native Win32 with WTL
WTL gives native message maps and lightweight wrapper classes without replacing Windows controls or adding a separately deployed UI runtime. Existing direct Win32 UI can be migrated incrementally.

## Structure
- `src/app/wit_gui`: WinMain, WTL application module lifetime, frame window, menus, status, command routing, native dialogs, browser/search panes, workflow controllers, scan coordination, and resources.
- `src/app/wit_win32`: reusable Win32/WTL base-window, base-dialog, DPI, virtual-list, and handle helpers.
- `src/modules/wit_types`: shared catalog, disk, folder, file, browser, search, and scan data types in the `wit::core` namespace.
- `src/modules/wit_catalog`: catalog/session abstractions and open-catalog state.
- `src/modules/wit_database`: SQLite connection, statement RAII, schema creation/validation, catalog writes, and catalog browsing repositories.
- `src/modules/wit_scanner`: filesystem scanning requests, interfaces, and implementation.
- `src/modules/wit_extractors`: optional metadata/archive extractor implementations.
- `src/modules/wit_search`: search abstractions and current SQLite-backed search execution.
- `src/modules/wit_infra`: settings, path, string, Win32 conversion, volume information, file-entry, result, and scope helpers.
- `sql`: table, index, and PRAGMA SQL used by the native initializer and tooling.

Database/search modules must not own window behavior. UI code must not encode SQLite schema DDL or durable read/write SQL; those belong in `wit_database`, `wit_search`, and root `sql` files.

## Scanning model
`FileScanner` runs on a worker thread, recursively enumerates with `FindFirstFileW/FindNextFileW`, skips inaccessible entries, and skips reparse-point directories in v1 to avoid loops. When requested by scan options, it uses the packaged libarchive runtime to validate and store readable archive contents as virtual folder subtrees while falling back to ordinary files for unreadable archives.

## SQLite model
`Database` opens the user-selected active SQLite catalog file, configures pragmas (`foreign_keys=ON`, `journal_mode=WAL`, `synchronous=NORMAL`), and creates replacement-format catalogs containing `catalog_metadata`, `disks`, `disk_scan_statistics`, `folders`, and `files`. Folders store recursive content sizes and a physical-directory/archive-backed type finalized by the scanner; latest statistics contain archive expansion counts. Folders and files are linked to disks with foreign keys and are written through prepared statements inside transactions. Old catalog formats and normalized catalogs lacking required folder totals or archive-aware fields are rejected rather than upgraded.

Timestamps are stored as Unix seconds, Boolean scan settings as `INTEGER` values constrained to `0` or `1`, and native file attributes as Win32 bitmasks. Catalog counts and storage totals are calculated from normalized tables; the current catalog database file size is read from the filesystem rather than stored in SQLite. `catalog.db` is only one possible catalog filename.

## Virtual ListView
Right pane uses `LVS_OWNERDATA` and only fetches pages (`LIMIT/OFFSET`) for visible ranges. A fixed-size page cache avoids loading all file rows into RAM, and folder sizes are read from stored scan totals rather than recursively calculated while browsing.

## Memory strategy
- No full in-memory file list for catalogs.
- Streaming insert during scan.
- Folder-size accumulation is transient to worker scanning; browser page data and ordering remain SQLite-backed.
- Small owner-data page cache.
- Simple RAII wrappers for deterministic cleanup.

## Future extension points
- Search dialog and SQL-backed filtering.
- Cancellation tokens for scanning.
- Sort modes and persistent UI state.
