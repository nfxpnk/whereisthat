# Building Where Is That?

Where Is That? is built with C++20, the Win32 API, SQLite, and the MSVC v143 toolset.

1. Install Visual Studio Build Tools 2022 or Visual Studio 2022.
2. Install **Desktop development with C++**.
3. Open `whereisthat.sln`.
4. Select `x64` platform.
5. Select `Release` configuration.
6. Build Solution.
7. Run `x64/Release/WhereIsThat.exe`.

The project links against the precompiled SQLite DLL in `third_party/sqlite`.
The post-build step copies `sqlite3.dll` next to `WhereIsThat.exe`.

## Command-line
From **Developer Command Prompt for VS 2022**:

`msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64 /m`
