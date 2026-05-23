# Storage Specification

## Persistence Model

Where Is That? persists catalogs in a local SQLite database, currently `catalog.db`. The database must retain enough filesystem metadata to list prior scans without accessing the original folder or drive.

SQLite is accessed in native C++ through the SQLite C API. Storage behavior belongs in `src/storage`; domain objects consumed by storage belong in `src/core`.

## Application Preferences

Application preferences are separate from catalog data and must not be added to SQLite merely to support UI configuration.

- General Settings persists implemented preferences in `settings.ini` beside `WhereIsThat.exe`.
- The initial preference is `[General] ShowStatusBar`, which defaults to enabled when the file or key is absent.
- UI preference reads and writes use a focused native platform helper; `src/storage` remains responsible for SQLite catalog persistence only.

## SQLite Packaging Decision

The following decision is mandatory:

- Keep `sqlite3.dll` separate from `WhereIsThat.exe`.
- Use `sqlite3.lib` as the MSVC import library at link time.
- Use `sqlite3.h` for compilation.
- Copy `sqlite3.dll` next to `WhereIsThat.exe` after the build.

SQLite must not be replaced with a managed data layer, a framework database abstraction, or a statically embedded amalgamation unless this specification is deliberately revised first.

## Current Data Responsibilities

- A catalog records its name, root path, creation timestamp, and item count.
- File rows record their catalog ownership, parent path, name, extension, size, modification timestamp, attributes, and whether the row represents a directory.
- File listing queries are scoped to a selected catalog and paged for the virtual ListView.
- Useful catalog lookup paths remain indexed as the data set grows.

## Storage Rules

- Use prepared statements for values supplied by application state or filesystem data.
- Manage SQLite handles and prepared statements with deterministic lifetime management.
- Keep schema creation and migrations in the storage layer.
- Treat on-disk databases from earlier application versions as user data; schema evolution must preserve or deliberately migrate existing catalogs.
- Do not execute SQLite reads or writes by embedding SQL in window/view classes.

## Operational Expectations

- Opening a database initializes the schema needed by the current application.
- Scanning writes catalog contents reliably and updates aggregate item count when scanning completes.
- Stored catalogs can be loaded at application start without requiring the scanned source path to be online.
