# Architecture

## Stack choice
Where Is That? uses C++20 + Win32 and the SQLite C API. This keeps the app small, native, and direct.

## Why not Qt/Electron/.NET
Those options increase runtime/dependency footprint and abstraction layers. The project goal is simple native Windows utility behavior and predictable memory/performance.

## Structure
- `src/app`: WinMain, frame window, menus, status, command routing.
- `src/core`: domain models, scanner, size formatting.
- `src/storage`: SQLite access and statement RAII wrapper.
- `src/ui`: list-view adapters for catalogs/files.
- `src/platform`: UTF-16/UTF-8 conversion, path/time helpers.

## Scanning model
`FileScanner` runs on a worker thread, recursively enumerates with `FindFirstFileW/FindNextFileW`, skips inaccessible entries, and skips reparse-point directories in v1 to avoid loops.

## SQLite model
`Database` opens `catalog.db`, configures pragmas (`foreign_keys=ON`, `journal_mode=WAL`, `synchronous=NORMAL`), creates schema/indexes, and writes files via prepared statements inside transactions.

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
