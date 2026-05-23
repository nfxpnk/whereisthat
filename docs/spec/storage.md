# Storage Specification

## Persistence Model

Where Is That? treats each user-selected SQLite database file as one catalog. A catalog database retains enough filesystem metadata to list prior scans without accessing the original folder or drive. A file named `catalog.db` is supported like any other catalog filename, but is not implicitly created or opened as the permanent catalog.

SQLite is accessed in native C++ through the SQLite C API. Storage behavior belongs in `src/storage`; domain objects consumed by storage belong in `src/core`.

## Application Preferences

Application preferences are separate from catalog data and must not be added to SQLite merely to support UI configuration.

- General Settings persists implemented preferences in `settings.ini` beside `WhereIsThat.exe`.
- The initial preference is `[General] ShowStatusBar`, which defaults to enabled when the file or key is absent.
- The optional `[General] LastCatalogPath` value records the most recently activated catalog database for startup restoration; an absent, empty, or unavailable path produces an empty startup state.
- The optional `[RecentCatalogs] Path1` through `Path10` values record successfully activated catalog database paths in most-recent-first order for `File > Open Recent`; entries are deduplicated without storing catalog data in the preferences file.
- UI preference reads and writes use a focused native platform helper; `src/storage` remains responsible for SQLite catalog persistence only.

## SQLite Packaging Decision

The following decision is mandatory:

- Keep `sqlite3.dll` separate from `WhereIsThat.exe`.
- Use `sqlite3.lib` as the MSVC import library at link time.
- Use `sqlite3.h` for compilation.
- Copy `sqlite3.dll` next to `WhereIsThat.exe` after the build.

SQLite must not be replaced with a managed data layer, a framework database abstraction, or a statically embedded amalgamation unless this specification is deliberately revised first.

## Current Data Responsibilities

- A catalog database contains indexed media-source records with name, root path, creation timestamp, and item count.
- File rows record their media-source ownership, parent path, name, extension, size, modification timestamp, attributes, and whether the row represents a directory.
- Browser listing queries are scoped to the virtual catalog root or to the immediate children of a selected stored source/folder location, and are paged for the virtual ListView.
- Folder-tree expansion queries return stored child directories only, keyed by source identity and persisted parent path; browsing never requires the indexed source to be online.
- Item-name search queries match literal text across all file and folder rows in the active catalog database and page results for the virtual search ListView.
- Useful media-source lookup paths remain indexed as the data set grows.
- Existing databases that use the established `catalogs` and `files` schema remain valid catalog files; the table name is retained for data compatibility.

## Storage Rules

- Use prepared statements for values supplied by application state or filesystem data.
- Manage SQLite handles and prepared statements with deterministic lifetime management.
- Keep schema creation and migrations in the storage layer.
- Treat on-disk databases from earlier application versions as user data; schema evolution must preserve or deliberately migrate existing catalogs.
- Do not execute SQLite reads or writes by embedding SQL in window/view classes.

## Operational Expectations

- Creating a catalog initializes a new SQLite database only at the user-selected new file path.
- Opening an existing catalog validates and initializes the supported schema without replacing an active catalog when opening fails.
- Scanning adds a new media source or replaces contents for the same source path in the active catalog only, and updates aggregate item count when scanning completes.
- Searching reads indexed file and folder metadata from the active catalog only and does not require the original scanned media to be available.
- The last-used available catalog can be loaded at application start without requiring any scanned source path to be online; otherwise startup has no active catalog.
