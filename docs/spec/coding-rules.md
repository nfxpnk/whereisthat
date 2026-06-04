# Coding Rules

## Required Technical Baseline

- Write application code in C++20.
- Target Windows 10/11 x64 only.
- Use the Unicode Win32 API and Windows Common Controls for the desktop application.
- Use vendored WTL 10.01/ATL native wrappers for the application message loop, main frame, migrated controls, and dialogs without changing the native UI platform.
- Build with MSVC v143 from Visual Studio/MSBuild project files.
- Use SQLite through `sqlite3.h` and the import-library/DLL deployment described in `storage.md` and `build.md`.

## Forbidden Technology Changes

Do not add or migrate application, UI, storage, or build functionality to:

- .NET, WPF, or C#.
- Qt or Electron.
- Python.
- CMake or vcpkg.

A proposal that would require any forbidden technology must first revise the canonical specification and be explicitly accepted as an architecture change.

## Module Ownership

- Put Windows application startup, main frame composition/routing, main-window collaborators, native dialogs, resources, and top-level lifetime in `src/app/wit_gui`.
- Put reusable Win32/WTL base-window, base-dialog, DPI, virtual-list, and handle helpers in `src/app/wit_win32`.
- Put reusable catalog, disk, folder, file, browser, search, and scan data types in `src/modules/wit_types`.
- Put open-catalog/session behavior in `src/modules/wit_catalog`.
- Put filesystem scanning behavior in `src/modules/wit_scanner` and extractor implementations in `src/modules/wit_extractors`.
- Put SQLite table/index DDL in the root `sql` directory. Keep SQLite connection, statements, catalog schema validation, and catalog browse/write queries in `src/modules/wit_database`.
- Put search abstractions and current SQLite-backed search execution in `src/modules/wit_search`.
- Put focused settings, path, string, Win32 conversion, filesystem, and volume helpers in `src/modules/wit_infra`.
- Do not bypass these ownership boundaries by placing durable SQL in UI code or control presentation inside database/search modules.

## Main Frame Rule

- Keep `MainFrame` limited to WTL-hosted top-level window lifetime, message-map command/message entry points, requested dialog/message presentation, and mechanical rendering of controller effects.
- Put child-control creation, toolbar, splitter/layout, status rendering, and presentation plumbing in `MainWindowChrome`.
- Put tree/list browsing, current location, and Back/Forward navigation state in `BrowserController`.
- Put catalog activation, Save/Discard/Close decisions, catalog command enablement, item-search eligibility, media scan initiation/adoption, and prompt continuations in `CatalogWorkflowController`.
- Put open catalog database collection/identity, per-catalog pending and dirty/protected state, settings/recent paths, and Save/discard/remove transitions in `CatalogSession`.
- Put scan worker lifetime, scan-in-progress state, staging work, and UI-thread completion payload ownership in `ScanCoordinator`.
- `CatalogWorkflowController` owns `CatalogSession` and `ScanCoordinator`; `MainFrame` must not inspect their state to choose an outcome.
- Add new main-window behavior to its owning component or justify a new focused component; do not grow unrelated responsibilities back into `MainFrame`.
- Keep WTL hosting and control-wrapper work within these existing component boundaries; WTL adoption is not a reason to merge their ownership.

## Native C++ Practices

- Prefer deterministic lifetime management for Win32, COM, thread, and SQLite resources.
- Prefer wide-character Win32 APIs at the Windows boundary; convert deliberately for SQLite UTF-8 data.
- Keep long-running scans off the UI thread and keep communication with windows thread-safe.
- Keep database queries parameterized with prepared statements.
- Preserve the paging approach for potentially large file listings.

## Change Discipline

- Keep documentation synchronized with intentional architecture, deployment, and supported-build changes.
- Add focused verification for changes that affect scanning, storage schema/queries, threading, or major UI behavior.
- Do not silently add a new runtime dependency or build prerequisite.
