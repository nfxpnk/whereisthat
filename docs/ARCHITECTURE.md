# Architecture

## Stack choice
Where Is That? uses C++20 + the Win32 API + WTL/ATL + SQLite + MSVC. The UI uses native Win32/Common Controls, with WTL/ATL wrappers introduced incrementally for windows and dialogs; storage goes through the SQLite C API, and builds are driven by the MSVC v143 toolset with MSBuild. This keeps the app small and native.

## Why native Win32 with WTL
WTL gives native message maps and lightweight wrapper classes without replacing Windows controls or adding a separately deployed UI runtime. Existing direct Win32 UI can be migrated incrementally.

## Structure
- `src/app`: WinMain, WTL application module lifetime, frame window, menus, status, command routing.
- `src/core`: domain models, scanner, size formatting.
- `src/storage`: SQLite access and statement RAII wrapper.
- `src/ui`: list-view adapters and native dialogs, with WTL wrappers where migrated.
- `src/platform`: UTF-16/UTF-8 conversion, path/time helpers.

## Scanning model
`FileScanner` runs on a worker thread, recursively enumerates with `FindFirstFileW/FindNextFileW`, skips inaccessible entries, and skips reparse-point directories in v1 to avoid loops.

## SQLite model
`Database` opens the user-selected active SQLite catalog file, configures pragmas (`foreign_keys=ON`, `journal_mode=WAL`, `synchronous=NORMAL`), and creates replacement-format catalogs containing `catalog_metadata`, `disks`, `disk_scan_statistics`, `folders`, and `files`. Folders and files are linked to disks with foreign keys and are written through prepared statements inside transactions. Old catalog formats are rejected rather than upgraded.

Timestamps are stored as Unix seconds, Boolean scan settings as `INTEGER` values constrained to `0` or `1`, and native file attributes as Win32 bitmasks. Catalog counts and storage totals are calculated from normalized tables; the current catalog database file size is read from the filesystem rather than stored in SQLite. `catalog.db` is only one possible catalog filename.

## Virtual ListView
Right pane uses `LVS_OWNERDATA` and only fetches pages (`LIMIT/OFFSET`) for visible ranges. A fixed-size page cache avoids loading all file rows into RAM.

## Memory strategy
- No full in-memory file list for catalogs.
- Streaming insert during scan.
- Small owner-data page cache.
- Simple RAII wrappers for deterministic cleanup.

## Future extension points
- Search dialog and SQL-backed filtering.
- Cancellation tokens for scanning.
- Sort modes and persistent UI state.
