# Database Documentation

## Engine And Format

Where Is That? uses SQLite through the native C API in `src/storage/Database.cpp`. Each user-selected SQLite file is one catalog. The current format is a replacement format: former databases containing the legacy `catalogs` and mixed folder/file `files` schema, and normalized catalogs without required stored folder content totals or archive-aware fields, are rejected and are not migrated.

`Database::InitializeSchema()` is the executable schema authority for new catalog files. Add/Update work is staged in an in-memory database copy and becomes durable only when Save backs that replacement-format database into the active file.

## Tables

| Table | Purpose | SQL | Field documentation |
|---|---|---|---|
| `catalog_metadata` | Singleton catalog-owned description metadata. | [schema/tables/catalog_metadata.sql](schema/tables/catalog_metadata.sql) | [tables/catalog_metadata.md](tables/catalog_metadata.md) |
| `disks` | One added disk/media source and native/storage metadata. | [schema/tables/disks.sql](schema/tables/disks.sql) | [tables/disks.md](tables/disks.md) |
| `disk_scan_statistics` | Latest successful scan statistics per disk, including readable archive counts. | [schema/tables/disk_scan_statistics.sql](schema/tables/disk_scan_statistics.sql) | [tables/disk_scan_statistics.md](tables/disk_scan_statistics.md) |
| `folders` | Normalized physical/archive-backed folder hierarchy and stored recursive file-byte totals for offline browsing. | [schema/tables/folders.sql](schema/tables/folders.sql) | [tables/folders.md](tables/folders.md) |
| `files` | File-only metadata stored in folders. | [schema/tables/files.sql](schema/tables/files.sql) | [tables/files.md](tables/files.md) |

## Relationships

- `disk_scan_statistics.disk_id -> disks.id ON DELETE CASCADE`.
- `folders.disk_id -> disks.id ON DELETE CASCADE`.
- `folders.parent_folder_id -> folders.id ON DELETE CASCADE`; a null parent marks the stored disk root folder.
- `files.disk_id -> disks.id ON DELETE CASCADE`.
- `files.folder_id -> folders.id ON DELETE CASCADE`.

`disks.source_path` is the current stored browse root and is case-insensitively unique. Add/Update matches this path for ordinary sources and can additionally match `disks.location` for an ISO image whose current mounted root has changed.

## Data Encodings

- All stored date/time fields are `INTEGER` Unix timestamps in seconds. `src/platform/Win32Helpers.cpp` converts filesystem times and formats display strings.
- Boolean `calculated_file_crcs` is stored as `INTEGER` `0` or `1`.
- File and folder attributes are stored as native Win32 bitmasks; relevant requested values are hidden, system, read-only, compressed, and archive flags.
- File extensions are stored without a dot, for example `txt`; files without an extension store `''`.
- `files.crc` is nullable and stores uppercase CRC-32 text only when CRC calculation succeeds during an enabled scan.
- `folders.content_size` stores the sum of file sizes in each folder and all stored descendants, finalized while scanning.
- `folders.entry_type` is `directory` for physical/internal directories and `archive` for a readable physical archive exposed as a folder.
- `disk_scan_statistics.scanned_archives`, `archive_files_count`, and `archive_folders_count` store latest successful archive expansion counts.
- `disks.disk_type` is constrained to `CD`, `DVD`, `BluRay`, `HardDisk`, `SolidStateDisk`, `RemovableUSB`, `VirtualDisk`, or `Other`. Mounted ISO sources are stored as `VirtualDisk`, Windows removable drives as `RemovableUSB`, and sources without a reliable subtype as `Other`.

## Stored And Derived Values

Stored disk scan results include `disks.total_files` and `disks.total_folders`, because those are metadata for the latest successful disk scan. Catalog-level totals are deliberately calculated by `Database::GetCatalogSummary()`:

| Catalog value | Source |
|---|---|
| Current catalog file size | Filesystem size of the active database path via `src/platform/VolumeInfo.cpp`; not stored in SQLite. |
| Total disks | `COUNT(disks)` |
| Total files | `COUNT(files)` |
| Total folders | `COUNT(folders)` |
| Total storage space | `SUM(disks.total_capacity)` |
| Total used space | `SUM(MAX(disks.total_capacity - disks.free_space, 0))`, expressed as a `CASE` query so anomalous free-space values cannot yield negative used space. |

## Schema Creation And Access

- New catalog creation initializes the tables and indexes above and inserts `catalog_metadata.id = 1`.
- Existing catalog open validates all required replacement-format columns, including `folders.content_size`, `folders.entry_type`, and archive scan counts; it includes no upgrade or backfill branch.
- SQLite configuration PRAGMAs are listed in [schema/pragmas.sql](schema/pragmas.sql).
- Defined indexes are listed in [schema/indexes.sql](schema/indexes.sql).
- No application views or triggers are defined: [schema/views.sql](schema/views.sql), [schema/triggers.sql](schema/triggers.sql).

## Updating This Documentation

When schema code changes, update the affected SQL/table pages and [schema-inventory.md](schema-inventory.md), then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/database-docs/verify.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/storage-smoke/run.ps1
```

The verifier checks documented table and field coverage plus shared DDL against the executable initializer. The smoke tool compiles the native storage/scanner sources into a temporary validation executable and exercises replacement-format behavior; narrative meanings still require review of the scanner and storage usage.
