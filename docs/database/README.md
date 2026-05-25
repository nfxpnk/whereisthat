# Database Documentation

## Engine And Ownership

Where Is That? stores catalog content in SQLite database files through the native SQLite C API. SQLite is linked through the separately deployed `third_party/sqlite/sqlite3.dll`; storage SQL is owned by `src/storage/Database.cpp` and prepared statement lifetime management by `src/storage/SQLiteStatement.cpp`.

Each user-selected SQLite file is one product catalog. Within it, the legacy-named `catalogs` table contains indexed media-source rows and `files` contains the scanned file and folder metadata beneath those sources. The repository file `catalog.db` is a possible catalog file, not an implicit application database.

## Open And Creation Flow

`src/app/MainFrame.cpp`, `MainFrame::ActivateCatalog()` directs creation and opening to `wit::storage::Database`:

- `Database::CreateNew()` creates a non-existing user-selected file and opens it for writable initialization.
- `Database::OpenExisting()` first attempts an editable open, then falls back to read-only opening for browseable protected catalogs.
- `Database::OpenInternal()` opens SQLite, validates existing files with `HasCatalogSchema()`, and calls `InitializeSchema()` only for editable connections.
- `Database::CreateWorkingCopy()` makes an in-memory SQLite backup used to stage Add/Update changes until Save.
- `Database::ReplaceCatalogDataFrom()` atomically replaces stored `catalogs` and `files` content from the staged copy when the user saves.

Application preferences, including recent/last catalog paths, are stored in `settings.ini` by `src/platform/AppSettings.cpp`, not in these database tables.

## Schema Creation And Upgrade

`Database::InitializeSchema()` is the canonical executable schema definition. For writable database connections it:

1. Enables foreign-key enforcement and applies WAL/NORMAL persistence settings.
2. Creates `catalogs` and `files` if absent.
3. Uses `PRAGMA table_info(files)` and conditionally adds `files.is_directory INTEGER NOT NULL DEFAULT 0` for an older schema that lacks the column.
4. Creates five query-supporting indexes if absent.

There is no numeric schema version, migration table, or `PRAGMA user_version` management in current source. The single observed upgrade is captured in [schema/migrations.sql](schema/migrations.sql), and its conditional execution remains implemented only in C++.

## Tables

| Table | Description | SQL | Fields and usage |
|---|---|---|---|
| `catalogs` | Indexed media-source roots contained in one catalog database file; name retained for compatibility. | [schema/tables/catalogs.sql](schema/tables/catalogs.sql) | [tables/catalogs.md](tables/catalogs.md) |
| `files` | Scanned file and directory metadata owned by a source and available for offline browse/search. | [schema/tables/files.sql](schema/tables/files.sql) | [tables/files.md](tables/files.md) |

## Relationships

`files.catalog_id` explicitly references `catalogs.id` with `ON DELETE CASCADE`. The application inserts a `files` directory row for a successfully inspected source root with `parent_path = ''`, and stores descendant hierarchy by matching `catalog_id` plus `parent_path`. The path hierarchy is an application-level relationship rather than a self-referential database constraint.

## Indexes And Query Shape

The schema defines [schema/indexes.sql](schema/indexes.sql):

| Index | Intended/currently visible query support |
|---|---|
| `idx_catalogs_root_path` | Case-insensitive source lookup and duplicate removal by `root_path`. |
| `idx_files_catalog_path` | Paged immediate-child listing and lazy directory expansion by source/path. |
| `idx_files_catalog_name` | Stored source/name ordering or future source name lookup; current global substring search does not filter by source. |
| `idx_files_catalog_extension` | Present in schema; no current extension-filter SQL was found. |
| `idx_files_catalog_size` | Present in schema; no current size-filter SQL was found. |

Item search uses `files.name LIKE ? ESCAPE '\'` with a leading wildcard and has no separately defined full-text or substring index.

## Additional Schema Artifacts

- [schema/pragmas.sql](schema/pragmas.sql) records connection/schema-initialization PRAGMAs.
- [schema/migrations.sql](schema/migrations.sql) records the conditional `is_directory` additive upgrade.
- [schema/triggers.sql](schema/triggers.sql) records that no application-defined triggers were found.
- [schema/views.sql](schema/views.sql) records that no application-defined views were found.
- [schema-inventory.md](schema-inventory.md) gives the complete inventory and source traceability.

## Keeping Documentation Current

When database behavior changes:

1. Update `src/storage/Database.cpp` first as the runtime source of truth, including upgrade handling for stored user data.
2. Update the matching table SQL and table Markdown file for every affected column, constraint, read, or write path.
3. Update shared index, PRAGMA, migration, trigger, or view files when those objects change.
4. Update this overview and `schema-inventory.md` for table lists, relationships, and compatibility notes.
5. Run `powershell -ExecutionPolicy Bypass -File tools/database-docs/verify.ps1` to check table coverage, field coverage, and documented DDL against the C++ initializer.

Detailed meanings remain intentionally human-authored: the verifier automates completeness and DDL drift checks without inventing semantics that are not expressed in code.

