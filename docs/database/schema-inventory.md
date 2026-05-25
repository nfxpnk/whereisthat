# Schema Inventory

## Authority And Scope

This inventory is derived from SQLite operations in `src/storage/Database.cpp`, with use-site tracing through `src/core/FileScanner.cpp`, `src/app/MainFrame.cpp`, and `src/ui`. A repository-wide search found no additional application SQL outside the storage layer. The checked-in `catalog.db` was not treated as an authority because the `sqlite3` command-line utility is not available in the development environment used for this documentation pass.

## Tables And Columns

| Table | Columns | Definition | Documentation |
|---|---|---|---|
| `catalogs` | `id`, `name`, `root_path`, `created_at`, `item_count` | [schema/tables/catalogs.sql](schema/tables/catalogs.sql) | [tables/catalogs.md](tables/catalogs.md) |
| `files` | `id`, `catalog_id`, `parent_path`, `name`, `extension`, `size`, `modified_at`, `attributes`, `is_directory` | [schema/tables/files.sql](schema/tables/files.sql) | [tables/files.md](tables/files.md) |

### Column Constraint Inventory

| Table.Column | Declared type and constraints | Relationship / application role |
|---|---|---|
| `catalogs.id` | `INTEGER PRIMARY KEY AUTOINCREMENT` | Parent key for source rows. |
| `catalogs.name` | `TEXT NOT NULL` | Media-source display name. |
| `catalogs.root_path` | `TEXT NOT NULL` | Application identity lookup, case-insensitively indexed; not unique. |
| `catalogs.created_at` | `TEXT NOT NULL` | New-source timestamp text. |
| `catalogs.item_count` | `INTEGER NOT NULL DEFAULT 0` | Application-maintained scan aggregate. |
| `files.id` | `INTEGER PRIMARY KEY AUTOINCREMENT` | Stored item identifier. |
| `files.catalog_id` | `INTEGER NOT NULL` | Explicit foreign key to `catalogs.id`. |
| `files.parent_path` | `TEXT NOT NULL` | Hierarchy/query scope; empty for stored root rows. |
| `files.name` | `TEXT NOT NULL` | Display and substring-search value. |
| `files.extension` | `TEXT NOT NULL` | Empty for directory rows in current writer. |
| `files.size` | `INTEGER NOT NULL` | Zero for directory rows in current writer. |
| `files.modified_at` | `TEXT NOT NULL` | Filesystem timestamp text. |
| `files.attributes` | `INTEGER NOT NULL DEFAULT 0` | Win32 attribute bit mask. |
| `files.is_directory` | `INTEGER NOT NULL DEFAULT 0` | Application writes Boolean-like `0`/`1`; added by conditional upgrade. |

## Indexes

| Name | Definition | Observed relationship to queries |
|---|---|---|
| `idx_catalogs_root_path` | `catalogs(root_path COLLATE NOCASE)` | Aligns with lookup/deletion predicates using `root_path=? COLLATE NOCASE`. |
| `idx_files_catalog_path` | `files(catalog_id, parent_path)` | Aligns with browser and child-folder predicates. |
| `idx_files_catalog_name` | `files(catalog_id, name)` | Defined; no current query combines `catalog_id` and `name` filtering. |
| `idx_files_catalog_extension` | `files(catalog_id, extension)` | Defined; current SQL has no extension filter. |
| `idx_files_catalog_size` | `files(catalog_id, size)` | Defined; current SQL has no size filter. |

All application-defined indexes are reproduced in [schema/indexes.sql](schema/indexes.sql).

## Foreign Keys And Inferred Relationships

| Child | Parent | Definition / confidence | Behavior |
|---|---|---|---|
| `files.catalog_id` | `catalogs.id` | Explicit: `FOREIGN KEY ... ON DELETE CASCADE` | A source deletion deletes its item rows while foreign keys are enabled. |
| `files.(catalog_id, parent_path)` | A source root or directory path | Inferred from `FileScanner::ScanFolder()`, `Database::GetBrowserItemsPage()`, and `Database::GetChildFolders()` | Encodes browsable hierarchy without a directory-row foreign key. |
| `catalogs.root_path` | Source selected by the user | Explicit use, application-enforced identity | Existing-source refresh searches case-insensitively and deletes duplicate source rows; no uniqueness constraint exists. |

## Triggers And Views

No `CREATE TRIGGER` or `CREATE VIEW` statement was found in application source. The corresponding inventory SQL files are [schema/triggers.sql](schema/triggers.sql) and [schema/views.sql](schema/views.sql).

SQLite may maintain internal objects such as `sqlite_sequence` because both table primary keys use `AUTOINCREMENT`; that internal SQLite behavior is not an application-defined table.

## PRAGMAs

| PRAGMA | Value | Invocation context | Source |
|---|---|---|---|
| `foreign_keys` | `ON` | Editable `InitializeSchema()` and read-only `OpenInternal()` | `src/storage/Database.cpp`, `Database::OpenInternal`, `Database::InitializeSchema` |
| `journal_mode` | `WAL` | Editable `InitializeSchema()` only | `src/storage/Database.cpp`, `Database::InitializeSchema` |
| `synchronous` | `NORMAL` | Editable `InitializeSchema()` only | `src/storage/Database.cpp`, `Database::InitializeSchema` |
| `table_info(files)` | query only | Tests whether the additive `is_directory` upgrade is required | `src/storage/Database.cpp`, `TableHasColumn`, `Database::InitializeSchema` |
| `table_info(catalogs)` / `table_info(files)` | query only | Validates required columns on existing open | `src/storage/Database.cpp`, `TableHasColumn`, `Database::HasCatalogSchema` |

The executable settings are recorded in [schema/pragmas.sql](schema/pragmas.sql). Query PRAGMAs are listed here rather than represented as schema configuration.

## Schema Upgrades And Version Metadata

| Mechanism | Details | Confidence |
|---|---|---|
| Conditional additive upgrade | If `TableHasColumn(db_, "files", "is_directory")` is false, writable initialization executes `ALTER TABLE files ADD COLUMN is_directory INTEGER NOT NULL DEFAULT 0;`. | Explicit |
| Schema validation | `HasCatalogSchema()` checks selected required columns using `PRAGMA table_info`; it does not validate types, declared constraints, indexes, or foreign keys. | Explicit |
| Numeric/version table | No `PRAGMA user_version`, schema version table, or ordered migration system was found. | Explicit absence in searched source |

## SQL Operation Inventory

| Operation | Tables / object | Source function(s) | Purpose |
|---|---|---|---|
| `CREATE TABLE`, `CREATE INDEX`, `ALTER TABLE`, configuration `PRAGMA` | both tables and all indexes | `Database::InitializeSchema()` | Initialize and upgrade writable catalog files. |
| `PRAGMA table_info` | both tables | `TableHasColumn()`, `Database::HasCatalogSchema()`, `Database::InitializeSchema()` | Validate or decide upgrade. |
| `INSERT` | `catalogs` | `Database::AddCatalog()`, `Database::ReplaceCatalogDataFrom()` | Add a new staged source or save staged sources. |
| `INSERT` | `files` | `Database::InsertFile()`, `Database::ReplaceCatalogDataFrom()` | Store scanned rows or save staged items. |
| `SELECT` | `catalogs` | `Database::FindCatalogByRootPath()`, `GetCatalogs()`, `GetBrowserItemCount()`, `GetBrowserItemsPage()`, `ReplaceCatalogDataFrom()` | Locate sources, display root, and copy staged data. |
| `SELECT` | `files` | `GetBrowserItemCount()`, `GetBrowserItemsPage()`, `GetChildFolders()`, `GetItemSearchCount()`, `GetItemSearchPage()`, `ReplaceCatalogDataFrom()` | Browse, expand, search, and copy staged data. |
| `UPDATE` | `catalogs` | `Database::UpdateCatalogItemCount()` | Store scan aggregate. |
| `DELETE` | `catalogs` | `Database::DeleteDuplicateCatalogsForRootPath()`, `ReplaceCatalogDataFrom()` | Deduplicate refresh identity or replace persisted staged data. |
| `DELETE` | `files` | `Database::DeleteFilesForCatalog()`, `ReplaceCatalogDataFrom()` | Refresh one source or replace persisted staged data. |

## Unused Objects And Potential Inconsistencies

- `idx_files_catalog_extension` and `idx_files_catalog_size` exist in the initializer, but no current SQL filter or ordering expression using those fields was found. They may be reserved for future browser sorting/filtering; this purpose is inferred.
- `idx_files_catalog_name` begins with `catalog_id`, whereas implemented item search filters globally with `name LIKE '%...%'`; no claim is made that this index accelerates that search.
- `catalogs.root_path` is used as case-insensitive source identity without a database uniqueness constraint. Duplicate cleanup is implemented during refresh, not guaranteed for arbitrary writes.
- `HasCatalogSchema()` does not check `files.is_directory`, although read queries select it. Editable open runs the conditional upgrade, but an older catalog that can be opened only read-only may pass validation and then fail when queries requiring `is_directory` are prepared. This is a possible compatibility bug inferred from the control flow.

