# Architecture Specification

## Architectural Contract

Where Is That? is a C++20, x64-only native Windows desktop application. The UI uses the Win32 API and Common Controls, with WTL/ATL native wrappers hosting the main frame, message loop, and migrated windows/dialogs; persistence is implemented with the SQLite C API loaded through a separately deployed `sqlite3.dll`. The Visual Studio/MSBuild project is the supported build definition.

The following alternatives are forbidden: .NET, WPF, C#, Qt, Electron, Python, CMake, and vcpkg.

## Layers

The source tree establishes these responsibilities:

| Directory | Responsibility |
| --- | --- |
| `src/app` | Entry point, WTL module/app lifetime, main frame composition/routing, window collaborators, Windows resources. |
| `src/ui` | Win32/Common Controls view adapters and dialog presentation, using WTL wrappers where migrated. |
| `src/core` | Catalog, disk, folder, file and scan-statistics models, scanning behavior, domain formatting. |
| `src/storage` | SQLite connection, schema, queries, statement/resource management. |
| `src/platform` | Win32-specific conversion, filesystem, path, and time helpers. |

Dependencies should flow from application/UI orchestration into core, storage, and platform helpers. Storage must not own window behavior, and UI code must not encode the SQLite schema or SQL queries.

## Main Window Responsibilities

`MainFrame` is the main-window composition root and top-level message/command entry point. It SHALL remain a reactive UI shell: it forwards workflow input to `CatalogWorkflowController`, presents requested native UI, and renders returned browser/chrome effects. It SHALL NOT decide catalog activation, Save/Discard/Close outcomes, media-scan eligibility or adoption, item-search eligibility, or catalog command availability.

`MainFrame` SHALL be hosted by `WTL::CFrameWindowImpl` and route its top-level native messages through a WTL message map. Application dispatch SHALL use a registered `WTL::CMessageLoop` with frame accelerator translation.

The established main-window collaborators are:

| Component | Responsibility |
| --- | --- |
| `MainWindowChrome` | Child controls, toolbar, splitter/layout, status rendering, and control-level presentation support. |
| `BrowserController` | Tree/list adapters, browser location and history, and synchronized navigation behavior. |
| `CatalogWorkflowController` | Main-window catalog/search/media workflow decisions, typed UI effects, and ownership of the session and scan collaborators. |
| `CatalogSession` | Collection of open catalog database sessions, selected active identity, per-catalog pending/dirty/protected state, settings/recent catalogs, Save and discard state transitions. |
| `ScanCoordinator` | Background scan worker lifetime, staged scan preparation, and UI-thread completion payloads. |

New main-window behavior SHALL extend the component that owns that responsibility, or introduce a separately justified focused component. Business workflow ordering belongs to `CatalogWorkflowController`; top-level native dispatch and mechanical effect rendering remain appropriate `MainFrame` responsibilities.

Modal dialog presentation for General Settings belongs to a dedicated WTL/ATL-hosted component in `src/ui`; `MainFrame` displays it when requested and returns accepted input to `CatalogWorkflowController` for persistence and resulting presentation effects.

## Runtime Shape

- `WhereIsThat.exe` is a Unicode Win32 GUI executable.
- The main window uses Common Controls for catalog browsing, file browsing, status display, and related native interactions.
- A scan is initiated from the UI and performs filesystem enumeration without blocking interactive window message handling.
- Scanned metadata is first staged in the selected destination open catalog session and is persisted to that user-selected SQLite catalog database only by explicit Save; `catalog.db` is not implicitly preferred over other filenames.
- The main frame can have no active catalog at startup, or can restore the available catalog identified by `settings.ini`.
- A catalog database stores catalog metadata, disk/media records, latest scan statistics, normalized folders with scan-time persisted recursive content sizes, and file-only records; browsing them remains possible after the original source drive is unavailable.
- The supported catalog format is deliberately new-format only and rejects earlier `catalogs`/mixed `files` databases and normalized catalogs lacking required folder content sizes rather than migrating them.
- Persisted catalog timestamps are Unix timestamp integers; user-facing formatting is performed outside SQLite.
- Catalog-wide totals are queried from normalized rows and catalog file size is read from the filesystem, rather than persisted as summary metadata.
- The main catalog browser presents a native TreeView with one top-level root per concurrently open database catalog and an owner-data ListView for the selected root or folder's immediate persisted contents, with Back/Forward and catalog-relative location display. File/folder pages read stored sizes through indexed immediate ownership relationships rather than browse-time descendant aggregation.
- The selected TreeView root, or the owning root of a selected descendant, identifies the active catalog for scoped commands and status.
- The catalog session component owns per-open-catalog editable/protected and pending/dirty state, exposes staged data to browsing through main-frame coordination, and supports independently closing catalogs or guarding shutdown when edits remain unsaved.
- The native status bar contains five independently updated sections for catalog state, protection, focus, selection totals, and extensible application-status lights.
- Large location lists are presented through an owner-data ListView backed by paged database access rather than loading an entire catalog into memory.

## Dependency Boundaries

- Win32 and Common Controls remain the UI platform; vendored WTL 10.01 headers and installed ATL provide lightweight native C++ wrappers.
- SQLite is the only database engine dependency.
- Third-party SQLite deployment consists of `sqlite3.h`, `sqlite3.lib`, and `sqlite3.dll` under `third_party/sqlite`.
- Third-party WTL integration consists of WTL 10.01 headers under `third_party/wtl/Include`; it does not add a deployed runtime DLL. `WTL::CAppModule`, `WTL::CMessageLoop`, `WTL::CFrameWindowImpl`, main-window control wrappers, and ATL/WTL-style dialogs are adopted within the component boundaries above.
- New dependencies require an explicit update to this specification before adoption.

## Authority

The files in `docs/spec/` are the persistent architectural contract. Documentation, implementation changes, and proposed features must conform to them or amend them deliberately before changing the architecture.
