# Acceptance Specification

## Architecture Acceptance

A change is acceptable only when Where Is That? remains:

- A native Windows desktop application for Windows 10/11 x64 only.
- Implemented in C++20 using the pure Win32 API and Windows Common Controls.
- Built with MSVC v143 through Visual Studio/MSBuild project files.
- Backed by SQLite through the native C API and a separately deployed `sqlite3.dll`.

The delivered application and supported build must not depend on .NET, WPF, C#, Qt, Electron, Python, CMake, vcpkg, or WTL.

## Build Acceptance Checklist

- `whereisthat.sln` builds in `Release|x64` using MSBuild and the v143 toolset.
- The executable output is `WhereIsThat.exe`.
- `sqlite3.dll` is copied to the same output directory as `WhereIsThat.exe`.
- The project links `sqlite3.lib` and compilation uses `sqlite3.h`.
- The supported build does not require CMake, vcpkg, or Python.

## Functional Acceptance Checklist

- The application opens as a native Windows desktop GUI on supported x64 Windows versions.
- A user can select a folder or disk and start a scan.
- Scan work does not freeze the primary UI message loop, and progress is surfaced in the interface.
- After restart, saved catalogs can be browsed from persisted data even when the scanned source is unavailable.
- File browsing remains database-backed and paged for large catalogs.

## Documentation Acceptance

- Architectural changes update `docs/spec/` before or together with implementation.
- `README.md` and the build guide continue to direct contributors to `docs/spec/` as the canonical architecture and acceptance source.
- Documentation must not describe a forbidden framework or build system as a supported path.
