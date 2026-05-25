# Acceptance Specification

## Architecture Acceptance

A change is acceptable only when Where Is That? remains:

- A native Windows desktop application for Windows 10/11 x64 only.
- Implemented in C++20 using the Win32 API, Windows Common Controls, and permitted WTL/ATL native wrappers.
- Built with MSVC v143 through Visual Studio/MSBuild project files.
- Backed by SQLite through the native C API and a separately deployed `sqlite3.dll`.

The delivered application and supported build must not depend on .NET, WPF, C#, Qt, Electron, Python, CMake, or vcpkg. WTL headers may be vendored and used with the MSVC ATL build component without adding a deployed UI runtime.

## Build Acceptance Checklist

- `whereisthat.sln` builds in `Release|x64` using MSBuild and the v143 toolset.
- The executable output is `WhereIsThat.exe`.
- `sqlite3.dll` is copied to the same output directory as `WhereIsThat.exe`.
- A published Windows release is an installer-free portable app folder/zip whose included PE binaries are directly SHA-256 Authenticode signed with timestamps and successfully verified from the final zip.
- The project links `sqlite3.lib` and compilation uses `sqlite3.h`.
- The project compiles vendored WTL headers with the required MSVC v143 ATL component and deploys no additional WTL runtime DLL.
- The supported build does not require CMake, vcpkg, or Python.

## Functional Acceptance Checklist

- The application opens as a native Windows desktop GUI on supported x64 Windows versions.
- Without a usable saved last-used database path, the application starts with no active catalog and does not create or open `catalog.db` implicitly.
- A user can create a fresh SQLite catalog file and open an existing SQLite catalog file, and activating either stores it as the last-used catalog for startup.
- New catalog files use normalized catalog metadata, disk, latest-statistics, folder, and file tables; former `catalogs`/mixed `files` databases are rejected without conversion or modification.
- Creating a catalog never modifies the previously active catalog or overwrites an existing file selected as a destination.
- Selecting Add/Update Disk Image from the menu, shared toolbar route, or `Ctrl+D` opens a modal `Add New Disk/Media` dialog with requested media-source, identity, option, action, and status controls; `OK` is disabled while no readable source is selected and Cancel starts no scan.
- A user can select a drive, network/computer folder, or natively resolvable ISO media source in that dialog and stage an addition or refresh in an editable active catalog only, without duplicate contents for the same indexed source.
- Stored disk and item timestamps use integer Unix timestamps, stored extensions omit a leading dot, optional CRC calculation writes nullable CRC values and latest-scan state, and native attribute flags remain available.
- Catalog totals are calculated from normalized rows and current catalog file size is read from the database file on disk rather than stored as a catalog total.
- Staged catalog changes are browseable but do not change the real active database until Save succeeds; failed saves retain pending work and `Modified` state.
- Switching catalogs or closing with pending changes offers Save, Discard, or Cancel, and protected/read-only catalogs remain browseable while rejecting edits.
- Scan work does not freeze the primary UI message loop, and program activity is surfaced through the status area.
- After restart, an available last-used catalog can be browsed from persisted data even when an indexed source is unavailable.
- The main browser displays an active-catalog root with folder-only TreeView expansion, a root disk inventory whose columns are `Disk Name`, `Media Type`, `Capacity`, `Free Space`, `Last Updated`, `Disk #`, `Description`, `Category`, and `Disk Location`, immediate file/folder contents below selected disks or folders, a catalog-relative address display, and synchronized Back/Forward navigation.
- Catalog-root disk inventory and file browsing remain database-backed and paged for large catalog locations, including when indexed media is unavailable.
- The status bar presents catalog state, protected state, focused item details, multi-selected item totals, and grey/green placeholder application lights, and continues to resize with the main window.

## Documentation Acceptance

- Architectural changes update `docs/spec/` before or together with implementation.
- `README.md` and the build guide continue to direct contributors to `docs/spec/` as the canonical architecture and acceptance source.
- Documentation must not describe a forbidden framework or build system as a supported path.
