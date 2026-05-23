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
- A published Windows release is an installer-free portable app folder/zip whose included PE binaries are directly SHA-256 Authenticode signed with timestamps and successfully verified from the final zip.
- The project links `sqlite3.lib` and compilation uses `sqlite3.h`.
- The supported build does not require CMake, vcpkg, or Python.

## Functional Acceptance Checklist

- The application opens as a native Windows desktop GUI on supported x64 Windows versions.
- Without a usable saved last-used database path, the application starts with no active catalog and does not create or open `catalog.db` implicitly.
- A user can create a fresh SQLite catalog file and open an existing SQLite catalog file, and activating either stores it as the last-used catalog for startup.
- Creating a catalog never modifies the previously active catalog or overwrites an existing file selected as a destination.
- A user can select a folder or disk image and add it to or refresh it in the active catalog only, without duplicate contents for the same indexed source.
- Scan work does not freeze the primary UI message loop, and progress is surfaced in the interface.
- After restart, an available last-used catalog can be browsed from persisted data even when an indexed source is unavailable.
- The main browser displays an active-catalog root with folder-only TreeView expansion, immediate file/folder contents, a catalog-relative address display, and synchronized Back/Forward navigation.
- File browsing remains database-backed and paged for large catalog locations.

## Documentation Acceptance

- Architectural changes update `docs/spec/` before or together with implementation.
- `README.md` and the build guide continue to direct contributors to `docs/spec/` as the canonical architecture and acceptance source.
- Documentation must not describe a forbidden framework or build system as a supported path.
