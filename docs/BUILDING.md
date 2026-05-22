# Building Where Is That?

1. Install Visual Studio 2022 or Visual Studio 2026.
2. Install **Desktop development with C++**.
3. Open `whereisthat.sln`.
4. Select `x64` platform.
5. Select `Release` configuration.
6. Build Solution.
7. Run `x64/Release/WhereIsThat.exe`.

The project links against the precompiled SQLite DLL in `third_party/sqlite`.
The post-build step copies `sqlite3.dll` next to `WhereIsThat.exe`.

## Command-line
From Developer Command Prompt for Visual Studio:

`msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64`
