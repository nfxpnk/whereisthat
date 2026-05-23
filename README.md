# Where Is That?

Where Is That? is a native Windows 10/11 file cataloging utility built with C++20, the Win32 API, SQLite, and MSVC. It scans a folder or drive, stores folder/file metadata in `catalog.db` (SQLite), and lets you browse catalogs later even when the original drive is disconnected.

## Why C++ + Win32 API + SQLite + MSVC
- Fast startup and low memory footprint.
- Small distributable.
- Direct control over Win32 API behavior.
- SQLite storage through the SQLite C API.
- MSVC toolchain and MSBuild project files.
- No .NET, WPF, Electron, Qt, or cross-platform runtime.

## Requirements
- Windows 10/11 x64
- Visual Studio Build Tools 2022 or Visual Studio 2022
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
From **Developer Command Prompt for VS 2022**:

`msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64 /m`

## Features (v1)
- Scan a folder or drive root into a catalog.
- Store both folder and file rows in SQLite.
- Background scanning thread.
- Status bar scan progress for folders and files.
- SQLite-backed catalog/file storage.
- Catalog list on left pane.
- Virtual owner-data file ListView on right pane.
- Reload catalogs and show saved database contents on restart.

## Limitations
- Search menu is placeholder.
- Splitter is static layout in v1.

## Roadmap
- Search dialog with indexed query.
- True draggable splitter.
- Better progress dialog and cancellation.
- Sorting and filtering in file view.
