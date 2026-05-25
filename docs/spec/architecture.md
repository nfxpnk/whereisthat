# Architecture Specification

## Architectural Contract

Where Is That? is a C++20, x64-only native Windows desktop application. The UI uses the Win32 API and Common Controls, with WTL/ATL native wrappers adopted incrementally for windows and dialogs; persistence is implemented with the SQLite C API loaded through a separately deployed `sqlite3.dll`. The Visual Studio/MSBuild project is the supported build definition.

The following alternatives are forbidden: .NET, WPF, C#, Qt, Electron, Python, CMake, and vcpkg.

## Layers

The source tree establishes these responsibilities:

| Directory | Responsibility |
| --- | --- |
| `src/app` | Entry point, WTL module/app lifetime, main frame window, command routing, Windows resources. |
| `src/ui` | Win32/Common Controls view adapters and dialog presentation, using WTL wrappers where migrated. |
| `src/core` | Catalog, disk, folder, file and scan-statistics models, scanning behavior, domain formatting. |
| `src/storage` | SQLite connection, schema, queries, statement/resource management. |
| `src/platform` | Win32-specific conversion, filesystem, path, and time helpers. |

Dependencies should flow from application/UI orchestration into core, storage, and platform helpers. Storage must not own window behavior, and UI code must not encode the SQLite schema or SQL queries.

## Runtime Shape

- `WhereIsThat.exe` is a Unicode Win32 GUI executable.
- The main window uses Common Controls for catalog browsing, file browsing, status display, and related native interactions.
- A scan is initiated from the UI and performs filesystem enumeration without blocking interactive window message handling.
- Scanned metadata is first staged in a working catalog session and is persisted to the currently active user-selected SQLite catalog database only by explicit Save; `catalog.db` is not implicitly preferred over other filenames.
- The main frame can have no active catalog at startup, or can restore the available catalog identified by `settings.ini`.
- A catalog database stores catalog metadata, disk/media records, latest scan statistics, normalized folders, and file-only records; browsing them remains possible after the original source drive is unavailable.
- The supported catalog format is deliberately new-format only and rejects earlier `catalogs`/mixed `files` databases rather than migrating them.
- Persisted catalog timestamps are Unix timestamp integers; user-facing formatting is performed outside SQLite.
- Catalog-wide totals are queried from normalized rows and catalog file size is read from the filesystem, rather than persisted as summary metadata.
- The main catalog browser presents a native TreeView rooted at the active database catalog and an owner-data ListView for the current folder's immediate persisted contents, with Back/Forward and catalog-relative location display.
- The main frame owns active-catalog editable/protected and pending/dirty state, exposes staged data to browsing, and guards catalog replacement or shutdown when edits remain unsaved.
- The native status bar contains five independently updated sections for catalog state, protection, focus, selection totals, and extensible application-status lights.
- Large location lists are presented through an owner-data ListView backed by paged database access rather than loading an entire catalog into memory.

## Dependency Boundaries

- Win32 and Common Controls remain the UI platform; vendored WTL headers and installed ATL provide lightweight native C++ wrappers.
- SQLite is the only database engine dependency.
- Third-party SQLite deployment consists of `sqlite3.h`, `sqlite3.lib`, and `sqlite3.dll` under `third_party/sqlite`.
- Third-party WTL integration consists of WTL 10.0 headers under `third_party/wtl/Include`; it does not add a deployed runtime DLL.
- New dependencies require an explicit update to this specification before adoption.

## Authority

The files in `docs/spec/` are the persistent architectural contract. Documentation, implementation changes, and proposed features must conform to them or amend them deliberately before changing the architecture.
