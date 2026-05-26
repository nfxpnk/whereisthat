# Schema Inventory

## Authority And Compatibility

The application schema is created and validated in `src/storage/Database.cpp`; values are populated primarily through `src/app/ScanCoordinator.cpp`, `src/core/FileScanner.cpp`, and `src/platform/VolumeInfo.cpp`. This inventory documents only the replacement catalog format. Old `catalogs`/mixed `files` files and normalized catalogs lacking required folder content totals or archive-aware fields are unsupported; no migration SQL exists.

## Tables And Columns

| Table | Columns | SQL | Documentation |
|---|---|---|---|
| `catalog_metadata` | `id`, `description` | [schema/tables/catalog_metadata.sql](schema/tables/catalog_metadata.sql) | [tables/catalog_metadata.md](tables/catalog_metadata.md) |
| `disks` | `id`, `disk_name`, `disk_number`, `source_path`, `volume_label`, `total_capacity`, `free_space`, `cluster_size`, `serial_number`, `file_system`, `total_files`, `total_folders`, `added_at`, `updated_at`, `description`, `category`, `location`, `disk_type` | [schema/tables/disks.sql](schema/tables/disks.sql) | [tables/disks.md](tables/disks.md) |
| `disk_scan_statistics` | `disk_id`, `last_scanned_at`, `image_scanning_time_ms`, `imported_descriptions_count`, `calculated_file_crcs`, `scanned_archives`, `archive_files_count`, `archive_folders_count` | [schema/tables/disk_scan_statistics.sql](schema/tables/disk_scan_statistics.sql) | [tables/disk_scan_statistics.md](tables/disk_scan_statistics.md) |
| `folders` | `id`, `disk_id`, `parent_folder_id`, `path`, `name`, `created_at`, `modified_at`, `accessed_at`, `attributes`, `content_size`, `entry_type` | [schema/tables/folders.sql](schema/tables/folders.sql) | [tables/folders.md](tables/folders.md) |
| `files` | `id`, `disk_id`, `folder_id`, `name`, `description`, `extension`, `crc`, `size`, `created_at`, `modified_at`, `accessed_at`, `attributes` | [schema/tables/files.sql](schema/tables/files.sql) | [tables/files.md](tables/files.md) |

## Indexes

| Index | Definition | Query responsibility |
|---|---|---|
| `idx_disks_source_path` | unique `disks(source_path COLLATE NOCASE)` | Current browse-root identity and duplicate prevention; ISO rescans may additionally match stored `location`. |
| `idx_folders_parent` | `folders(disk_id, parent_folder_id)` | Immediate child folder retrieval. |
| `idx_folders_disk_path` | unique `folders(disk_id, path COLLATE NOCASE)` | Path-scoped navigation lookup/invariant. |
| `idx_files_folder` | `files(folder_id)` | Files contained in one folder. |
| `idx_files_disk_name` | `files(disk_id, name)` | Disk-scoped stored name access. |
| `idx_files_extension` | `files(extension)` | Normalized extension access. |

No CRC index is defined because no CRC lookup query is implemented.

## Constraints And Relationships

| Object | Constraint / relationship |
|---|---|
| `catalog_metadata.id` | `PRIMARY KEY CHECK (id=1)` singleton row. |
| `disks.source_path` | Case-insensitively unique source identity. |
| `disks.disk_type` | Allowed-value `CHECK`: `CD`, `DVD`, `BluRay`, `HardDisk`, `SolidStateDisk`, `RemovableUSB`, `VirtualDisk`, `Other`. |
| `disk_scan_statistics.disk_id` | Primary and foreign key to `disks.id`, cascade delete. |
| `disk_scan_statistics.calculated_file_crcs` | Boolean `CHECK (0,1)`. |
| `disk_scan_statistics.scanned_archives`, `archive_files_count`, `archive_folders_count` | Required non-negative counts for successfully expanded archives in the latest scan. |
| `folders.disk_id` | Foreign key to `disks.id`, cascade delete. |
| `folders.parent_folder_id` | Nullable self foreign key to `folders.id`, cascade delete. |
| `folders.content_size` | Required recursive stored-file byte total finalized during scanning. |
| `folders.entry_type` | Required allowed value `directory` or `archive`; an archive row remains in the folder hierarchy. |
| `files.disk_id` | Foreign key to `disks.id`, cascade delete. |
| `files.folder_id` | Foreign key to `folders.id`, cascade delete. |

## PRAGMAs, Triggers, Views And Versions

| Category | Inventory |
|---|---|
| Configuration PRAGMAs | `foreign_keys=ON`, `journal_mode=WAL`, `synchronous=NORMAL`; see [schema/pragmas.sql](schema/pragmas.sql). |
| Validation PRAGMA | `PRAGMA table_info(...)` is used by C++ to recognize required replacement-format columns. |
| Triggers | None defined by the application. |
| Views | None defined by the application. |
| Schema/version metadata | No numeric schema-version table or `PRAGMA user_version` management exists; format recognition is structural and new-format only. |
| Migrations | None. Former catalog formats and normalized catalogs missing required folder-size or archive-aware fields are rejected without alteration. |

## Read And Write Traceability

| Area | Code entry points | Stored objects |
|---|---|---|
| Initialization / validation | `Database::InitializeSchema`, `Database::HasCatalogSchema` | All tables, indexes, metadata singleton. |
| Staged save | `Database::CreateWorkingCopy`, `Database::ReplaceCatalogDataFrom` | Whole replacement-format database through SQLite backup. |
| Disk add/rescan | `ScanCoordinator::Start`, `Database::AddDisk`, `FindDiskBySourcePath`, `DeleteContentForDisk`, `UpdateDisk` | `disks`, folder/file replacement. |
| Latest statistics | `FileScanner::ScanFolder`, `Database::UpdateDiskScanStatistics` | `disk_scan_statistics`. |
| File/folder capture | `FileScanner::ScanFolder`, `Database::InsertFolder`, `UpdateFolderContentSize`, `InsertFile` | `folders`, `files`, including scan-time folder totals and readable archive-backed subtrees. |
| Disk native metadata | `src/platform/VolumeInfo.cpp` | Volume, capacity, cluster and filesystem fields on `disks`. |
| Browser/search reads | `Database::GetCatalogs`, `GetBrowserItemCount`, `GetBrowserItemsPage`, `HasChildFolders`, `GetChildFolders`, `GetItemSearchCount`, `GetItemSearchPage` | `disks`, `folders`, `files`. |
| Summary reads | `Database::GetCatalogSummary` | Derived counts/sums plus filesystem file size. |
