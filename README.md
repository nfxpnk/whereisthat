# Where Is That?

Where Is That? is a native Windows 10/11 file cataloging utility built with C++20, the Win32 API, SQLite, and MSVC. It scans folders or drives into user-selected SQLite catalog files, then lets you browse and search indexed items later even when the original drive is disconnected.

## Project Specification
The canonical product, architecture, build, storage, UI, coding, and acceptance rules are maintained in [`docs/spec/`](docs/spec/). Contributors should read and preserve these decisions when proposing or implementing changes.

## Why C++ + Win32 API + SQLite + MSVC
- Fast startup and low memory footprint.
- Small distributable.
- Direct control over Win32 API behavior.
- SQLite storage through the SQLite C API.
- MSVC toolchain and MSBuild project files.
- No .NET, WPF, C#, Qt, Electron, Python, CMake, vcpkg, WTL, or cross-platform runtime.

## Requirements
- Windows 10/11 x64
- Visual Studio Build Tools 2022, Visual Studio 2022, or Visual Studio 2026 with MSVC v143 installed
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
- Search for indexed files and folders by name with paged results.
- Reopen the last-used available catalog database on startup.

## Limitations
- Search commands other than Search for Items are placeholders.

## Roadmap
- Better progress dialog and cancellation.
- Sorting and filtering in file view.
