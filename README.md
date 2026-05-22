# Where Is That?

Where Is That? is a native Windows 10/11 file cataloging utility. It scans a folder, stores file metadata in `catalog.db` (SQLite), and lets you browse catalogs later even when the original drive is disconnected.

## Why native C++/Win32
- Fast startup and low memory footprint.
- Small distributable.
- Direct control over Win32 behavior.
- No .NET, WPF, Electron, Qt, or cross-platform runtime.

## Requirements
- Windows 10/11 x64
- Visual Studio 2022 or Visual Studio 2026
- Desktop development with C++ workload
- SQLite 3.53.1 DLL/import library/header are vendored under `third_party/sqlite`

## Build (Visual Studio)
1. Open `whereisthat.sln`.
2. Select `Release` + `x64`.
3. Build Solution.
4. Run `x64/Release/WhereIsThat.exe`.

The project links against the precompiled SQLite DLL in `third_party/sqlite`.
The post-build step copies `sqlite3.dll` next to `WhereIsThat.exe`.

## Build (MSBuild)
From Developer Command Prompt for Visual Studio:

`msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64 /m`

## Features (v1)
- Create catalog from folder.
- Background scanning thread.
- SQLite-backed catalog/file storage.
- Catalog list on left pane.
- Virtual owner-data file ListView on right pane.
- Reload catalogs on restart.

## Limitations
- Search menu is placeholder.
- Splitter is static layout in v1.

## Roadmap
- Search dialog with indexed query.
- True draggable splitter.
- Better progress dialog and cancellation.
- Sorting and filtering in file view.
