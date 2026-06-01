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
- The application uses its WTL 10.01 message loop and WTL-hosted main frame without adding a UI runtime dependency.
- The supported build does not require CMake, vcpkg, or Python.

## Functional Acceptance Checklist

- The application opens as a native Windows desktop GUI on supported x64 Windows versions.
- Without a usable saved last-used database path, the application starts with no active catalog and does not create or open `catalog.db` implicitly.
- A user can create a fresh SQLite catalog file and open existing SQLite catalog files concurrently as independent top-level TreeView roots, and selecting a newly opened root stores it as the last-used catalog for startup.
- New catalog files use normalized catalog metadata, disk, latest-statistics, folder, and file tables, including required recursive stored folder content sizes, archive-backed folder type data, and archive counts; former `catalogs`/mixed `files` databases and normalized catalogs missing required fields are rejected without conversion or modification.
- Creating or opening a catalog never modifies or closes another open catalog and does not duplicate an already open equivalent path; New Catalog may replace a closed existing destination only after native overwrite confirmation, and refuses any destination already open in the application.
- Selecting Add/Update Disk Image from the menu, shared toolbar route, or `Ctrl+D` opens a modal `Add New Disk/Media` dialog with requested media-source, identity, option, action, and status controls; `OK` is disabled while no readable source is selected and Cancel starts no scan.
- A user can select a drive, network/computer folder, or natively resolvable ISO media source in that dialog, choose an editable open destination catalog immediately after `Disk name:`, and stage an addition or refresh only there, without duplicate contents for the same indexed source.
- Stored disk and item timestamps use integer Unix timestamps, stored extensions omit a leading dot, optional CRC calculation writes nullable CRC values and latest-scan state, native attribute flags remain available, and each successful scan persists recursive stored folder content byte totals.
- When Browse inside compressed files is selected, readable physical archives are stored as marked folder-like entries with correctly nested member rows and latest archive counts; unreadable, corrupt, encrypted, or unsupported archives are logged and retained as files without failing the scan.
- Catalog totals are calculated from normalized rows and current catalog file size is read from the database file on disk rather than stored as a catalog total.
- Staged catalog changes are browseable but do not change the real active database until Save succeeds; failed saves retain pending work and `Modified` state.
- File > Close targets the catalog owning the current tree selection; root context-menu Close is root-only, prompts with safe-default No, and on a modified catalog offers Save, Discard, or Cancel while leaving unrelated roots untouched.
- Each open catalog retains independent pending state; protected/read-only catalogs remain browseable while rejecting edits, and application exit resolves pending work in all modified open catalogs.
- Scan work does not freeze the primary UI message loop, and program activity is surfaced through the status area.
- Menu, toolbar, accelerator, notification, splitter, status rendering, and scan completion routes remain functional through the WTL-hosted main-frame dispatch path.
- After restart, an available last-used catalog can be browsed from persisted data even when an indexed source is unavailable.
- The main browser displays one root per open catalog with folder-only TreeView expansion scoped to its database, a selected-root disk inventory whose columns are `Disk Name`, `Media Type`, `Capacity`, `Free Space`, `Last Updated`, `Disk #`, `Description`, `Category`, and `Disk Location`, immediate file/folder contents below selected disks or folders, a catalog-relative address display, and synchronized Back/Forward navigation.
- Displayed folder sizes use persisted scan totals, and paging immediate file/folder contents does not calculate recursive folder totals during browsing.
- Expanded archives can be entered through ordinary tree/list navigation and remain browseable from stored rows while indexed media is unavailable.
- The main browser renders packaged transparent 16x16 Fugue type icons before displayed names: `database-sql.png` for open catalog-database roots, `drive.png` for indexed disk/media tree and inventory rows, `folder.png` for ordinary folders, `document.png` for non-archive files, and `vise-drawer.png` for retained archive files and archive-backed folders.
- New or replacement scans preserve `VirtualDisk` and `RemovableUSB` classification, persist Windows CD/DVD drive volumes as optical media, and persist `HardDisk` or `SolidStateDisk` for fixed volumes only when the native Windows seek-penalty query reports that distinction reliably; otherwise they retain `Other`.
- Catalog-root disk inventory and file browsing remain database-backed and paged for large catalog locations, including when indexed media is unavailable.
- The status bar presents catalog state, protected state, focused item details, multi-selected item totals, and grey/green placeholder application lights, and continues to resize with the main window.

## Documentation Acceptance

- Architectural changes update `docs/spec/` before or together with implementation.
- `README.md` and the build guide continue to direct contributors to `docs/spec/` as the canonical architecture and acceptance source.
- Documentation must not describe a forbidden framework or build system as a supported path.
