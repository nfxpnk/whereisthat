# Build And Deployment Specification

## Supported Build Environment

- Host and target: Windows 10/11 x64.
- Language: C++20.
- Compiler/toolset: MSVC v143.
- IDE: Visual Studio 2022 or Visual Studio 2026 with MSVC v143, Desktop development with C++, and C++ ATL for latest v143 build tools (x86 & x64) installed.
- Build system: MSBuild through `whereisthat.sln` and `WhereIsThat.vcxproj`.
- Supported configurations: `Debug|x64` and `Release|x64`.

The project must not introduce CMake, vcpkg, Python-based build steps, or a replacement application toolchain.

## Build Commands

Visual Studio builds open `whereisthat.sln`, select an x64 configuration, and build the solution.

Command-line builds run from a Visual Studio Developer Command Prompt:

```bat
msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64 /m
```

For a development build, substitute `Debug` for `Release`.

## WTL And ATL Rule

- Include vendored WTL 10.0 headers from `third_party/wtl/Include`.
- Use ATL supplied by the installed MSVC v143 ATL component.
- Keep WTL/ATL as native compile-time support; no additional WTL runtime DLL is deployed.

## SQLite Link And Deployment Rule

SQLite must remain dynamically deployed:

- Include `third_party/sqlite/sqlite3.h` when compiling storage code.
- Link with `third_party/sqlite/sqlite3.lib` as the MSVC import library.
- Keep `third_party/sqlite/sqlite3.dll` as a separate runtime DLL.
- Copy `sqlite3.dll` next to `WhereIsThat.exe` after every successful build.
- Do not statically compile SQLite into `WhereIsThat.exe`.

The expected release runtime output includes:

```text
x64/Release/WhereIsThat.exe
x64/Release/sqlite3.dll
```

## Build Acceptance

A build is acceptable when all of the following are true:

- The solution builds successfully with MSBuild using `Release|x64` and MSVC v143.
- The project compiles as C++20 and targets the Windows desktop x64 platform only.
- The output contains `WhereIsThat.exe` and a separate `sqlite3.dll` in the same directory.
- Linking resolves SQLite through `sqlite3.lib`, and application storage compilation uses `sqlite3.h`.
- The application starts on a supported Windows x64 system with no framework runtime such as .NET, WPF, Qt, Electron, or Python required; WTL/ATL integration adds no separate deployed UI runtime.
- No CMake or vcpkg files or steps become required for the supported build.
