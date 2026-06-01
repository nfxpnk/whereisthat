# Building Where Is That?

Where Is That? is built with C++20, the Win32 API, WTL/ATL, SQLite, and the MSVC v143 toolset.

The canonical build, dependency, and acceptance rules are in [`spec/build.md`](spec/build.md) and [`spec/acceptance.md`](spec/acceptance.md). All build changes must remain consistent with the project specification in [`spec/`](spec/).

1. Install Visual Studio Build Tools 2022, Visual Studio 2022, or Visual Studio 2026 with MSVC v143 installed.
2. Install **Desktop development with C++** and **C++ ATL for latest v143 build tools (x86 & x64)**.
3. Open `whereisthat.sln`.
4. Select `x64` platform.
5. Select `Release` configuration.
6. Build Solution.
7. Run `x64/Release/WhereIsThat.exe`.

The project links against the precompiled SQLite DLL in `third_party/sqlite`.
The post-build step copies `sqlite3.dll` next to `WhereIsThat.exe`.
The root `sql` schema files are embedded into `WhereIsThat.exe` as resources at build time.
WTL 10.0 is vendored as headers in `third_party/wtl/Include` and relies on the MSVC ATL component; it adds no separately deployed runtime.

## Command-line
From **Developer Command Prompt for VS 2022**:

`msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64 /m`
