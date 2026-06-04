# Tests

This folder contains the GoogleTest foundation for WhereIsThat. The Visual
Studio solution includes a separate `UnitTests` console project alongside the
main `WhereIsThat` Win32/WTL application project.

## GoogleTest Dependency

GoogleTest is intentionally not tracked in this repository. `UnitTests.vcxproj`
expects a local checkout at:

```text
third_party\googletest
```

Restore it before building tests:

```powershell
git clone --branch v1.14.0 --depth 1 https://github.com/google/googletest.git third_party\googletest
```

The path is ignored by `.gitignore`, so the GoogleTest source files stay local
and should not appear in commits. This keeps the repository small while still
avoiding CMake, vcpkg, NuGet, and global Visual Studio extensions. The test
project builds GoogleTest directly from:

```text
third_party\googletest\googletest\src\gtest-all.cc
third_party\googletest\googletest\src\gtest_main.cc
```

Best practice note: if the project later needs reproducible dependency restore
in CI, use a Git submodule or a small restore script pinned to the same tag. For
now, the ignored local checkout is the least invasive option for this legacy
MSBuild setup.

## Building Tests

Open `whereisthat.sln` in Visual Studio and build the `UnitTests` project, or
run MSBuild from a Visual Studio developer environment:

```powershell
msbuild UnitTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

The test project uses the same x64 Debug/Release configurations, v143 toolset,
Unicode character set, and C++20 language standard as the main app. It links the
same sqlite and libarchive dependencies needed by the migrated storage smoke
test and copies the required DLLs after build.

## Running Tests

After building `UnitTests`, run the executable from its output folder:

```powershell
.\x64\Debug\UnitTests.exe
```

To run only the storage smoke tests:

```powershell
.\x64\Debug\UnitTests.exe --gtest_filter=StorageSmoke.*
```

For a fresh command-line build and run, use the same direct project/executable
flow:

```powershell
msbuild UnitTests.vcxproj /p:Configuration=Debug /p:Platform=x64
.\x64\Debug\UnitTests.exe
```

This is intentionally plain MSBuild plus the GoogleTest executable. There are no
custom test runner scripts.

## Storage Smoke Migration

The old standalone smoke tool has been migrated into GoogleTest:

```text
tests\unit\storage\StorageSmokeTests.cpp
```

The migrated test is named:

```text
StorageSmoke.CatalogDatabaseScannerAndCoordinatorIntegration
```

It covers the old `tools\storage-smoke\StorageSmoke.cpp` behavior as a broad
smoke/integration test:

- path display and volume disk-type classification helpers
- catalog creation, working-copy staging, metadata updates, and persistence
- disk group and disk insert/update behavior
- file scanning with and without CRC calculation
- cooperative scan cancellation rollback behavior
- browser root/item/search projections after source media removal
- scan statistics persistence
- `ScanCoordinator` completed, cancelled, failed, detached-target, and rapid
  close/cancel lifecycle behavior
- archive fixture creation, readable/empty/broken/unsafe archive handling,
  archive diagnostics, virtual archive browsing, and archive scan statistics
- archive-disabled scanning behavior
- old/invalid schema rejection paths without accidental schema mutation

The old files under `tools\storage-smoke` are no longer needed and were removed.
New storage smoke coverage should be added to
`tests\unit\storage\StorageSmokeTests.cpp` or split into smaller GoogleTest
files under `tests\unit\storage`.

## Adding Tests

Add small, focused GoogleTest files under `tests\unit`. Add storage-related
tests under `tests\unit\storage`. Include new `.cpp` files in `UnitTests.vcxproj`
so Visual Studio and MSBuild both compile them.
