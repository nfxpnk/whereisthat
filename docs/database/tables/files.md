# `files`

## Purpose

`files` stores offline-browsable metadata for every scanned item under a source, including directories. A successfully read source root is also inserted as a directory row with an empty `parent_path`.

## Lifecycle

- Created by `wit::storage::Database::InsertFile()` as `FileScanner::ScanFolder()` enumerates a source root and its descendants.
- Read by paged browser, child-folder expansion, and item-search queries in `Database`.
- Deleted for one source before an existing-source rescan through `Database::DeleteFilesForCatalog()`.
- Deleted automatically when its owning `catalogs` row is deleted while `PRAGMA foreign_keys=ON` is active.
- During explicit Save, all persisted file rows are deleted and copied from the staged working database inside a transaction.

## Keys And Relationships

- Primary key: `id`.
- Foreign key: `catalog_id REFERENCES catalogs(id) ON DELETE CASCADE`.
- Navigation relationship: rows sharing `catalog_id` and a stored `parent_path` are retrieved as immediate browser children; this path relationship is application-managed, not a foreign key.

## Indexes

| Index | Columns | Use visible in code |
|---|---|---|
| `idx_files_catalog_path` | `catalog_id`, `parent_path` | Supports immediate-child counts/pages and child-folder expansion. |
| `idx_files_catalog_name` | `catalog_id`, `name` | Defined for per-source name access; current global substring search does not constrain `catalog_id`. |
| `idx_files_catalog_extension` | `catalog_id`, `extension` | No currently implemented SQL query using extension filtering was found. |
| `idx_files_catalog_size` | `catalog_id`, `size` | No currently implemented SQL query using size filtering was found. |

## Constraints And Assumptions

- All fields are `NOT NULL`; `attributes` and `is_directory` default to `0`.
- `is_directory` was added through the only implemented conditional schema upgrade; older writable databases are upgraded when opened.
- `is_directory` is written as `1` for folders and `0` for files by current code. The schema does not constrain other integer values.
- `attributes` stores the Win32 file attribute bit mask obtained during scanning.
- Directory rows receive an empty `extension` and a `size` of `0` in `FileScanner::ScanFolder()`.
- `modified_at` is stored in UTC text formatted as `YYYY-MM-DDTHH:MM:SSZ` by `wit::platform::FileTimeToIso8601()`.

## Related Code

| Source | Functions / classes | Role |
|---|---|---|
| `src/storage/Database.cpp` | `InitializeSchema`, `HasCatalogSchema`, `InsertFile`, `DeleteFilesForCatalog`, `GetBrowserItemCount`, `GetBrowserItemsPage`, `GetChildFolders`, `GetItemSearchCount`, `GetItemSearchPage`, `ReplaceCatalogDataFrom` | Defines schema and all SQL reads/writes. |
| `src/core/FileEntry.h` | `wit::core::FileEntry` | In-memory row representation. |
| `src/core/FileScanner.cpp` | `FileScanner::ScanFolder` | Populates field values from Win32 enumeration. |
| `src/core/BrowserLocation.h` | `wit::core::BrowserLocation` | Holds source/path query scope used for hierarchy reads. |
| `src/ui/FileListView.cpp` | `FileListView::SetLocation`, `EnsurePage`, `TextFor` | Displays paged item fields. |
| `src/ui/CatalogTreeView.cpp` | `CatalogTreeView::Expand` | Reads directory rows to lazily populate the tree. |
| `src/ui/SearchDialog.cpp` | `SearchDialog::Search`, `EnsurePage`, `TextFor` | Displays item-name search results. |
| `src/app/MainFrame.cpp` | `OnFileActivate`, `UpdateFocusedItemStatus`, `UpdateSelectionSummaryStatus`, `OnSaveCatalog`, `OnAddOrUpdateDiskImage` | Navigates folder rows, presents item metadata, and stages/persists mutations. |
| `src/platform/PathHelpers.cpp`, `src/platform/Win32Helpers.cpp` | `FileExtension`, `FileTimeToIso8601` | Computes stored extensions and timestamp text. |

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description | Expected values / assumptions | Used in code | Confidence |
|---|---|---:|---|---|---|---|---|---|
| `id` | `INTEGER` | No | none | `PRIMARY KEY AUTOINCREMENT` | Unique stored item-row identifier. | SQLite-generated on scan insert; copied explicitly during Save. | `src/storage/Database.cpp`: `ReplaceCatalogDataFrom`, `GetBrowserItemsPage`, `GetChildFolders`, `GetItemSearchPage` | Explicit |
| `catalog_id` | `INTEGER` | No | none | `FOREIGN KEY` to `catalogs.id`; first column of four composite indexes | Owner source identifier for the scanned item. | Must identify an existing source while foreign keys are enabled. | `src/storage/Database.cpp`: `InsertFile`, `DeleteFilesForCatalog`, browse/tree/search queries, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder`; `src/app/MainFrame.cpp`: `OnFileActivate` | Explicit |
| `parent_path` | `TEXT` | No | none | Indexed in `idx_files_catalog_path` after `catalog_id` | Stored containing path used for immediate-child hierarchy retrieval. | Empty for the stored source-root directory row; filesystem parent path for descendants. | `src/storage/Database.cpp`: `InsertFile`, browser/tree/search queries, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder`; `src/ui/FileListView.cpp`: `TextFor`; `src/ui/SearchDialog.cpp`: `TextFor` | Explicit |
| `name` | `TEXT` | No | none | Indexed in `idx_files_catalog_name` after `catalog_id` | Display/file name of the scanned item. | Root row uses source display name; searched via escaped substring `LIKE`. | `src/storage/Database.cpp`: `InsertFile`, browser/tree/search queries, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder`; `src/ui/FileListView.cpp`: `TextFor`; `src/ui/SearchDialog.cpp`: `TextFor` | Explicit |
| `extension` | `TEXT` | No | none | Indexed in `idx_files_catalog_extension` after `catalog_id` | Filename extension used for item type display. | Set to empty text for directories; for files, `FileExtension()` stores text from the final `.` onward, including the dot, or empty text if absent. | `src/storage/Database.cpp`: `InsertFile`, browser/tree/search queries, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder`; `src/platform/PathHelpers.cpp`: `FileExtension`; `src/ui/FileListView.cpp`: `TextFor`; `src/ui/SearchDialog.cpp`: `TextFor` | Explicit |
| `size` | `INTEGER` | No | none | Indexed in `idx_files_catalog_size` after `catalog_id` | File length stored for display and selection totals. | Win32 64-bit file size for files; set to `0` for directories. | `src/storage/Database.cpp`: `InsertFile`, browser/tree/search queries, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder`; `src/ui/FileListView.cpp`: `TextFor`; `src/ui/SearchDialog.cpp`: `TextFor`; `src/app/MainFrame.cpp`: status calculations | Explicit |
| `modified_at` | `TEXT` | No | none | none | Stored last-modified timestamp for display. | UTC text formatted as `YYYY-MM-DDTHH:MM:SSZ` by `wit::platform::FileTimeToIso8601()`. | `src/storage/Database.cpp`: `InsertFile`, browser/tree/search queries, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder`; `src/platform/Win32Helpers.cpp`: `FileTimeToIso8601`; `src/ui/FileListView.cpp`: `TextFor`; `src/ui/SearchDialog.cpp`: `TextFor`; `src/app/MainFrame.cpp`: `UpdateFocusedItemStatus` | Explicit |
| `attributes` | `INTEGER` | No | `0` | none | Win32 filesystem attributes captured at scan time. | Bit mask sourced from `dwFileAttributes`; no persisted enum/check constraint. | `src/storage/Database.cpp`: `InsertFile`, browser/tree/search queries, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder` | Explicit |
| `is_directory` | `INTEGER` | No | `0` | none | Indicates whether the row represents a directory. | Current writer binds `1` for directories and `0` for files; no SQL `CHECK` constraint. | `src/storage/Database.cpp`: conditional upgrade, `InsertFile`, browser/tree/search queries, `ReplaceCatalogDataFrom`; `src/core/FileScanner.cpp`: `ScanFolder`; `src/ui/FileListView.cpp`: `TextFor`; `src/ui/SearchDialog.cpp`: `TextFor`; `src/app/MainFrame.cpp`: navigation/status | Explicit |
