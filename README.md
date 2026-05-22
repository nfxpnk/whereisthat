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

## Build (Visual Studio)
1. Open `whereisthat.sln`.
2. Select `Release` + `x64`.
3. Build Solution.
4. Run `bin/x64/Release/WhereIsThat.exe`.

## Build (MSBuild)
`msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64`

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
- Offline environment placeholder SQLite amalgamation file must be replaced with official upstream amalgamation before production build.

## Roadmap
- Search dialog with indexed query.
- True draggable splitter.
- Better progress dialog and cancellation.
- Sorting and filtering in file view.
