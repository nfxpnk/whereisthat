# Storage Specification

## Persistence Model

Where Is That? treats each user-selected SQLite database file as one catalog. The current catalog format is new-format only: database files created with the former `catalogs` and mixed folder/file `files` tables, or normalized catalogs lacking required stored folder content sizes or archive-aware fields, are not supported and are not migrated or modified when opening fails validation.

SQLite is accessed in native C++ through the SQLite C API. Catalog storage behavior belongs in `src/modules/wit_database`; search-specific SQLite reads currently belong in `src/modules/wit_search`; domain objects consumed by those modules belong in `src/modules/wit_types`; Win32 filesystem and volume metadata discovery belongs in `src/modules/wit_infra` or scan orchestration.

## Application Preferences

Application preferences are separate from catalog data and must not be added to SQLite merely to support UI configuration.

- General Settings persists implemented preferences in `settings.ini` beside `WhereIsThat.exe`, including `General/ShowToolbar` defaulting to enabled, `General/DoNotShowAlphaWarning` defaulting to disabled, and `General/MainSplitterPosition` with a default of `360` when the key is missing.
- `[General] LastCatalogPath` and `[RecentCatalogs] Path1` through `Path10` record activated catalog file paths, not catalog content.
- UI preference reads and writes use `src/modules/wit_infra`; `src/modules/wit_database` remains responsible only for SQLite catalog persistence and must not own window behavior.

## SQLite Packaging Decision

- Keep `sqlite3.dll` separate from `WhereIsThat.exe`.
- Use `sqlite3.lib` as the MSVC import library at link time.
- Use `sqlite3.h` for compilation.
- Copy `sqlite3.dll` next to `WhereIsThat.exe` after the build.

SQLite must not be replaced with a managed data layer, framework database abstraction, or statically embedded amalgamation unless this specification is deliberately revised.

## Archive Packaging Decision

- Use the vendored libarchive headers and `archive.lib` import library for archive scanning.
- Keep libarchive and its dependency DLLs separate from `WhereIsThat.exe`.
- Copy the libarchive runtime DLLs next to `WhereIsThat.exe` after the build.

Archive scanning must not require a system-wide libarchive installation or introduce a replacement build system/dependency manager.

## Current Data Responsibilities

- `catalog_metadata` contains the one catalog-owned free-text description record.
- `disks` contains one indexed disk/media source with source identity, volume/capacity metadata, stored per-scan disk totals, descriptive classification, disk type, and added/updated timestamps.
- `disk_scan_statistics` contains only the latest successful scan statistics for each disk, including archive expansion counts.
- `folders` contains persisted physical and archive-backed folder hierarchy, metadata, an archive/directory type marker, and scan-time recursive stored-file byte totals for offline browsing.
- `files` contains file-only records with folder ownership, description, extension without a dot, optional CRC, size, timestamps, and native attribute flags.
- Foreign keys and cascading deletes keep disk content and latest statistics associated with their owning disk.

## Encoding Rules

- Persist all date/time values as SQLite `INTEGER` Unix timestamps in seconds. Format user-visible timestamp strings only at the presentation boundary.
- Persist Boolean values such as `calculated_file_crcs` as `INTEGER` `0` or `1`.
- Persist file/folder Win32 attributes as the native integer bitmask, including hidden, system, read-only, compressed, and archive flags when present.
- Persist each folder content size as the sum of stored file byte sizes in that folder and all stored descendant folders, finalized during successful scanning rather than calculated during browsing.
- Persist folder entry type as `directory` or `archive`; a readable physical archive uses `archive` while its internal directory descendants use `directory`.
- Persist file extensions without the leading dot and use empty text for no extension.
- Persist file CRC as nullable text; it remains null when CRC calculation is disabled or a CRC is unavailable.
- Persist disk type as exactly one of `CD`, `DVD`, `BluRay`, `HardDisk`, `SolidStateDisk`, `RemovableUSB`, `VirtualDisk`, or `Other`; mounted ISO-file sources resolve to `VirtualDisk`, Windows CD/DVD drive volumes resolve to `CD`, removable volumes resolve to `RemovableUSB`, and fixed volumes resolve to `HardDisk` or `SolidStateDisk` only when a native seek-penalty device query reports the distinction reliably. Unavailable or unsafe classification resolves to `Other`.
- Retain the accepted original media location so a mounted image can be matched for rescan even when Windows assigns it a different current browse root.

## Derived Catalog Values

Catalog-wide summary values are not cached in SQLite:

- Read catalog file size from the active database file on the filesystem.
- Calculate total disks from `COUNT(disks)`.
- Calculate total files from `COUNT(files)`.
- Calculate total folders from `COUNT(folders)`.
- Calculate total storage space from `SUM(disks.total_capacity)`.
- Calculate total used space from the non-negative sum of `disks.total_capacity - disks.free_space`, treating an anomalous disk with free space exceeding capacity as zero used space.

`disks.total_files` and `disks.total_folders` are persisted per-disk results for the latest successful scan because they are disk metadata; they are not the source of catalog-wide summary counts.

## Storage Rules

- Use prepared statements for values supplied by application state or filesystem data.
- Manage SQLite handles and prepared statements with deterministic lifetime management.
- Keep schema creation and supported-format validation centralized in `src/modules/wit_database`, with table/index DDL sourced from the root `sql` directory.
- Reject former-format catalog files and normalized catalog files without required stored folder content sizes or archive-aware fields without attempting upgrade, conversion, or backfill.
- Do not execute SQLite reads or writes by embedding SQL in window/view classes.
- Keep hierarchy/list/search reads paged and available offline from persisted data.
- Read immediate folder/file contents using persisted item sizes and indexed ownership relationships; do not calculate recursive descendant totals during owner-data list display.
- Read catalog-root disk inventory rows directly from persisted `disks` metadata using count and paged queries; do not retrieve live media information while browsing.
- Keep contents page ordering in SQLite so later column sorting can use the same persisted numeric file and folder sizes.

## Operational Expectations

- Creating a catalog initializes only a new SQLite database at the user-selected new path.
- Opening an existing catalog validates the replacement schema; a readable new-format protected catalog can be opened for browsing without mutation.
- Scanning stages a disk addition or source replacement in a session-local working database only.
- Save commits pending replacement-format data atomically; Discard drops only pending changes.
- A successful new disk scan stores `added_at` and `updated_at`; a successful rescan retains its original `added_at`, advances `updated_at`, replaces indexed content, and updates latest statistics.
- A successful scan finalizes and stages each stored folder's recursive file-byte total in the same pending catalog data as its folder/file replacement.
- When archive browsing is selected, a successfully read physical archive is staged as an archive-backed folder with stored member folders/files and latest archive counts; an unreadable, corrupt, unsupported, or encrypted archive is logged and retained as an ordinary file while scanning continues.
- Native scanning captures available volume information, capacity/free-space information, filesystem metadata, folder/file times, and native attributes; unavailable classifications are not invented.
