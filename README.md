# Where Is That?

Where Is That? is a native Windows 10/11 file cataloging utility built with C++20, the Win32 API, WTL/ATL, SQLite, and MSVC. It scans folders or drives into user-selected SQLite catalog files, then lets you browse and search indexed items later even when the original drive is disconnected.

## Project Specification
The canonical product, architecture, build, storage, UI, coding, and acceptance rules are maintained in [`docs/spec/`](docs/spec/). Contributors should read and preserve these decisions when proposing or implementing changes.

## Why C++ + Win32 API + WTL + SQLite + MSVC
- Fast startup and low memory footprint.
- Small distributable.
- Direct control over Win32 API behavior.
- Lightweight WTL/ATL message-map and native-window wrappers for UI code.
- SQLite storage through the SQLite C API.
- MSVC toolchain and MSBuild project files.
- No .NET, WPF, C#, Qt, Electron, Python, CMake, vcpkg, or cross-platform runtime.

## Requirements
- Windows 10/11 x64
- Visual Studio Build Tools 2022, Visual Studio 2022, or Visual Studio 2026 with MSVC v143 installed
- Desktop development with C++ workload and the **C++ ATL for latest v143 build tools (x86 & x64)** component
- SQLite 3.53.1 DLL/import library/header are vendored under `third_party/sqlite`
- WTL 10.0 headers are vendored under `third_party/wtl/Include`

## Build (Visual Studio)
1. Open `whereisthat.sln`.
2. Select `Release` + `x64`.
3. Build Solution.
4. Run `x64/Release/WhereIsThat.exe`.

The project links against the precompiled SQLite DLL in `third_party/sqlite`.
The post-build step copies `sqlite3.dll` next to `WhereIsThat.exe`.
WTL is header-only; it uses the ATL headers supplied by the installed MSVC ATL component.

## Build (MSBuild)
From **Developer Command Prompt for VS 2022**:

`msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64 /m`

## Features (v1)
- Scan a folder or drive root into a catalog.
- Store normalized disk, latest scan-statistics, folder, and file records in SQLite.
- Background scanning thread with staged changes and explicit Save/Discard handling.
- Add New Disk/Media entry dialog for drive, folder/network, and mounted ISO sources.
- Status bar catalog state, protection indication, focused-item details, selection totals, and activity lights.
- SQLite-backed catalog/file storage.
- Explorer-style catalog browser with a folder TreeView on the left and current-location contents on the right.
- Back/Forward navigation and a catalog-relative address display.
- Virtual owner-data contents ListView with database-backed paging.
- Search for indexed files and folders by name with paged results.
- Reopen the last-used available catalog database on startup.

## Limitations
- Search currently supports literal item-name substring matching only.
- Most commands already shown in the File, Edit, View, Search, Actions, Options, Window, and Help menus or toolbar are presentation placeholders.
- Add New Disk/Media exposes archive, CRC, description, auto-save, and related scan options as placeholders; they do not yet change scanning.

## Roadmap

### Phase 1: Complete the catalog browser foundation
- Show real scan counts/progress and support cancellation without losing the saved catalog or a valid pending edit.
- Implement existing File placeholders: Save As, Save All Catalogs, Import XML, Close All, Rebuild Catalog Database, and Catalogs Info/statistics.
- Make item-list sorting, reverse order, column configuration, and useful detail view choices functional and persistent.
- Implement basic selected-item actions: Properties metadata display, Open in Explorer, View/Launch when original media is available, Rename source, and Remove from Catalog with staged-save semantics.
- Complete smoke tests for protected catalogs, recent catalogs, pending Save/Discard/Cancel, Add/Update source variants, and offline browsing.

### Phase 2: Store the organizing metadata
- Extend the replacement SQLite format for thumbnails, flags/tags, colors, and optional lending state as those features are implemented.
- Provide Properties and Catalog Setup dialogs for editing those values, including multi-selection updates.
- Add a description/thumbnail pane and a focused description editor/assistant workflow.
- Use stored media identity for reliable targeted Update Disk Image rather than identifying updates only by selected root path.

### Phase 3: Turn search into the main retrieval tool
- Extend quick search to descriptions, masks/wildcards, selectable scopes, item types, and previous-result narrowing.
- Add advanced criteria for size/date/attributes/categories/flags/media metadata with saved expressions and Boolean combinations.
- Implement Find in This Catalog/selected location, Find Selected Items, and duplicate detection.
- Implement comparison workflows: catalog versus current media, filesystem selection versus catalog, and two cataloged groups.
- Add a session user list, with optional saved lists in a catalog, as an input for searches, batch edits, and reports.

### Phase 4: Enrich scanning and stored content
- Make scan options functional: folder limitation rules, CRC/hash calculation, update policies, auto-save choice, and completion actions.
- Import descriptions and thumbnails during scanning, first through built-in handlers for common text, image, document, and audio metadata sources.
- Expand ISO and network/UNC media handling while retaining offline browsing and protected/read-only catalog behavior.

### Phase 5: Reporting, exchange, and extensibility
- Implement Report Generator over the browser, search results, and user lists, with selectable columns, ordering, preview/print, and CSV/text/HTML export first.
- Support description import/export formats and whole-catalog interchange where it remains useful.
- Add broader application settings, portable configuration selection, localization support, and opt-in automation/batch update facilities.
