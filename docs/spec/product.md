# Product Specification

## Purpose

Where Is That? is a native Windows desktop utility that creates a persistent catalog of files and folders. A user can scan a folder or disk, disconnect the original storage, and later browse the saved catalog.

## Supported Product Surface

- Target operating systems: Windows 10 and Windows 11, x64 only.
- Application type: native desktop executable, `WhereIsThat.exe`.
- Primary workflow: select a filesystem root, scan it into a local catalog, and browse catalog contents.
- Persistence: a local SQLite database named `catalog.db`.

## Non-Goals And Prohibited Platforms

This project is intentionally a native Windows application. It must not be migrated to, embedded in, or supplemented by:

- .NET, WPF, or C#.
- Qt, Electron, or WTL.
- Python application or build tooling.
- CMake or vcpkg.
- Cross-platform UI or runtime frameworks.

## Canonical Technology Decisions

- Language standard: C++20.
- UI platform: pure Win32 API with Windows Common Controls.
- Storage engine: SQLite accessed through its C API.
- Database deployment: `sqlite3.dll` remains a separate DLL deployed beside the executable.
- Compiler/toolset: MSVC v143.
- IDE support: Visual Studio 2022 and Visual Studio 2026 with the required MSVC v143 workload installed.
- Command-line build support: MSBuild using the Visual Studio solution and project files.

The other files in this directory define the implementation and acceptance rules for these decisions.
