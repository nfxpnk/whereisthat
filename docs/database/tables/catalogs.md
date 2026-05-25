# `catalogs`

## Purpose

`catalogs` stores the indexed media sources contained in a single user-selected catalog database file. The name is retained for compatibility with older application behavior; current product terminology treats the SQLite file itself as the catalog and each row here as a source root.

## Lifecycle

- Created by `wit::storage::Database::AddCatalog()` when `MainFrame::OnAddOrUpdateDiskImage()` scans a source whose normalized root path is not already present.
- Read by `Database::GetCatalogs()` and root-level browser queries to populate the catalog tree and root list.
- Updated by `Database::UpdateCatalogItemCount()` after scanning files and folders for a source.
- Existing-source refresh retains the chosen matching source row, deletes its `files`, and rescans it. Any additional rows matched by the same case-insensitive `root_path` are deleted by `Database::DeleteDuplicateCatalogsForRootPath()`.
- During explicit Save, `Database::ReplaceCatalogDataFrom()` deletes persisted rows and copies staged source rows back with their original IDs inside a transaction.

## Keys And Relationships

- Primary key: `id`.
- Referenced by the explicit foreign key `files.catalog_id -> catalogs.id ON DELETE CASCADE`.
- Inferred logical identity for refresh: `root_path` compared with `COLLATE NOCASE`; it is not protected by a `UNIQUE` constraint.

## Indexes

| Index | Columns / expression | Use visible in code |
|---|---|---|
| `idx_catalogs_root_path` | `root_path COLLATE NOCASE` | Supports `Database::FindCatalogByRootPath()` and `Database::DeleteDuplicateCatalogsForRootPath()`. |

## Constraints And Assumptions

- Every declared field is `NOT NULL`; `item_count` defaults to `0`.
- `id` is an SQLite `INTEGER PRIMARY KEY AUTOINCREMENT`.
- `item_count` is an application-maintained aggregate of inserted folder and file records after a scan. No database constraint verifies it against `files`.
- No schema constraint prevents two rows for the same `root_path`; deduplication occurs in the staged update workflow.
- `created_at` is populated from `wit::platform::NowIso8601()` when a source row is first created, in UTC text formatted as `YYYY-MM-DDTHH:MM:SSZ`.

## Related Code

| Source | Functions / classes | Role |
|---|---|---|
| `src/storage/Database.cpp` | `InitializeSchema`, `HasCatalogSchema`, `AddCatalog`, `FindCatalogByRootPath`, `DeleteDuplicateCatalogsForRootPath`, `UpdateCatalogItemCount`, `GetCatalogs`, `GetBrowserItemCount`, `GetBrowserItemsPage`, `ReplaceCatalogDataFrom` | Defines schema and all SQL reads/writes. |
| `src/core/Catalog.h` | `wit::core::Catalog` | In-memory row representation. |
| `src/core/FileScanner.cpp` | `FileScanner::ScanFolder` | Produces the aggregate `item_count` update after inserting child records. |
| `src/app/MainFrame.cpp` | `OnAddOrUpdateDiskImage`, `ReloadBrowser`, `OnSaveCatalog` | Creates/refreshes staged sources, displays them, and persists staged rows. |
| `src/ui/CatalogTreeView.cpp` | `CatalogTreeView::Reload` | Uses source identity, name, and root path for navigation nodes. |
| `src/platform/Win32Helpers.cpp` | `NowIso8601` | Formats the stored source creation timestamp. |

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description | Expected values / assumptions | Used in code | Confidence |
|---|---|---:|---|---|---|---|---|---|
| `id` | `INTEGER` | No | none | `PRIMARY KEY AUTOINCREMENT`; referenced by `files.catalog_id` | Persisted source-row identifier. | Positive SQLite-generated rowid on ordinary insertion; IDs are copied explicitly during Save. | `src/storage/Database.cpp`: `AddCatalog`, `GetCatalogs`, `GetBrowserItemsPage`, `ReplaceCatalogDataFrom`; `src/app/MainFrame.cpp`: `OnAddOrUpdateDiskImage`; `src/ui/CatalogTreeView.cpp`: `Reload` | Explicit |
| `name` | `TEXT` | No | none | none | Display name of the indexed media source. | Supplied from accepted disk/media name or source-derived display name when a new source is added. | `src/storage/Database.cpp`: `AddCatalog`, `GetCatalogs`, `GetBrowserItemsPage`, `ReplaceCatalogDataFrom`; `src/app/MainFrame.cpp`: `OnAddOrUpdateDiskImage`; `src/ui/CatalogTreeView.cpp`: `Reload` | Explicit |
| `root_path` | `TEXT` | No | none | Indexed by `idx_catalogs_root_path` with `COLLATE NOCASE` | Normalized filesystem/source path used to locate and identify an indexed source. | Made absolute with `GetFullPathNameW()` and stripped of trailing slash characters except drive roots; compared case-insensitively during refresh; not schema-unique. | `src/storage/Database.cpp`: `AddCatalog`, `FindCatalogByRootPath`, `DeleteDuplicateCatalogsForRootPath`, `GetCatalogs`, `GetBrowserItemsPage`, `ReplaceCatalogDataFrom`; `src/app/MainFrame.cpp`: `NormalizedSourcePath`, `OnAddOrUpdateDiskImage`; `src/ui/CatalogTreeView.cpp`: `Reload` | Explicit |
| `created_at` | `TEXT` | No | none | none | Timestamp recorded when a new source row is created. | UTC text formatted as `YYYY-MM-DDTHH:MM:SSZ` by `wit::platform::NowIso8601()`. | `src/storage/Database.cpp`: `AddCatalog`, `GetCatalogs`, `GetBrowserItemsPage`, `ReplaceCatalogDataFrom`; `src/app/MainFrame.cpp`: `OnAddOrUpdateDiskImage`; `src/platform/Win32Helpers.cpp`: `NowIso8601` | Explicit |
| `item_count` | `INTEGER` | No | `0` | none | Stored count of scanned item rows belonging to the source. | Set to `fileCount + folderCount` after successful scanning; consistency is maintained in application code. | `src/storage/Database.cpp`: `AddCatalog`, `UpdateCatalogItemCount`, `GetCatalogs`, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder` | Explicit |
