# Coding Rules

## Required Technical Baseline

- Write application code in C++20.
- Target Windows 10/11 x64 only.
- Use the Unicode Win32 API and Windows Common Controls for the desktop application.
- Build with MSVC v143 from Visual Studio/MSBuild project files.
- Use SQLite through `sqlite3.h` and the import-library/DLL deployment described in `storage.md` and `build.md`.

## Forbidden Technology Changes

Do not add or migrate application, UI, storage, or build functionality to:

- .NET, WPF, or C#.
- Qt, Electron, or WTL.
- Python.
- CMake or vcpkg.

A proposal that would require any forbidden technology must first revise the canonical specification and be explicitly accepted as an architecture change.

## Module Ownership

- Put Windows application startup, main frame routing, resources, and top-level lifetime in `src/app`.
- Put native view adapters and dialog UI behavior in `src/ui`.
- Put reusable application models and scanning/domain operations in `src/core`.
- Put SQLite schema, connection, statements, and queries in `src/storage`.
- Put focused Windows platform conversion or filesystem helpers in `src/platform`.
- Do not bypass these ownership boundaries by placing SQL in UI code or control presentation inside storage code.

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
