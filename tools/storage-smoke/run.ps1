[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vcVars = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path -LiteralPath $vcVars)) {
    throw "MSVC x64 build environment not found: $vcVars"
}

$tempBase = [System.IO.Path]::GetTempPath()
$buildDir = Join-Path $tempBase ("whereisthat-storage-smoke-build-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $buildDir | Out-Null
try {
    $exe = Join-Path $buildDir "storage-smoke.exe"
    $response = Join-Path $buildDir "compile.rsp"
    @(
        "/nologo"
        "/std:c++20"
        "/EHsc"
        "/DUNICODE"
        "/D_UNICODE"
        "/I`"$root`""
        "/I`"$(Join-Path $root 'src')`""
        "/I`"$(Join-Path $root 'src\modules\wit_types\include')`""
        "/I`"$(Join-Path $root 'src\modules\wit_infra\include')`""
        "/I`"$(Join-Path $root 'src\modules\wit_database\include')`""
        "/I`"$(Join-Path $root 'src\modules\wit_catalog\include')`""
        "/I`"$(Join-Path $root 'src\modules\wit_scanner\include')`""
        "/I`"$(Join-Path $root 'src\modules\wit_extractors\include')`""
        "/I`"$(Join-Path $root 'src\modules\wit_search\include')`""
        "/I`"$(Join-Path $root 'src\app\wit_win32\include')`""
        "/I`"$(Join-Path $root 'src\app\wit_gui\include')`""
        "/I`"$(Join-Path $root 'src\app\wit_gui\res')`""
        "/I`"$(Join-Path $root 'third_party\sqlite')`""
        "/I`"$(Join-Path $root 'third_party\libarchive\include')`""
        "/I`"$(Join-Path $root 'third_party\wtl\Include')`""
        "`"$(Join-Path $root 'tools\storage-smoke\StorageSmoke.cpp')`""
        "`"$(Join-Path $root 'src\app\wit_gui\src\ScanCoordinator.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_scanner\src\FileScanner.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_database\src\CatalogSchema.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_database\src\Database.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_database\src\SqliteBrowserRepository.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_database\src\SqliteConnection.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_database\src\SQLiteStatement.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_extractors\src\ArchiveExtractor.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_extractors\src\AudioExtractor.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_extractors\src\ImageExtractor.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_infra\src\FileEntry.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_infra\src\PathHelpers.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_infra\src\StringUtils.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_infra\src\VolumeInfo.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_infra\src\Win32Helpers.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_search\src\QueryBuilder.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_search\src\SearchExecutor.cpp')`""
        "`"$(Join-Path $root 'src\modules\wit_search\src\SqliteSearchExecutor.cpp')`""
        "`"$(Join-Path $root 'third_party\sqlite\sqlite3.lib')`""
        "`"$(Join-Path $root 'third_party\libarchive\lib\archive.lib')`""
        "user32.lib"
        "ole32.lib"
        "uuid.lib"
        "windowscodecs.lib"
        "advapi32.lib"
        "shell32.lib"
        "/Fe:`"$exe`""
    ) | Set-Content -LiteralPath $response -Encoding ASCII

    Push-Location $buildDir
    try {
        $compile = "call `"$vcVars`" >nul && cl.exe @`"$response`""
        & cmd.exe /d /c $compile
        if ($LASTEXITCODE -ne 0) { throw "Storage smoke compilation failed." }
        Copy-Item -LiteralPath (Join-Path $root "third_party\sqlite\sqlite3.dll") -Destination $buildDir
        Copy-Item -Path (Join-Path $root "third_party\libarchive\bin\*.dll") -Destination $buildDir
        & $exe
        if ($LASTEXITCODE -ne 0) { throw "Storage smoke execution failed." }
    } finally {
        Pop-Location
    }
} finally {
    $resolved = [System.IO.Path]::GetFullPath($buildDir)
    $resolvedTemp = [System.IO.Path]::GetFullPath($tempBase)
    if ($resolved.StartsWith($resolvedTemp, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolved).StartsWith("whereisthat-storage-smoke-build-")) {
        Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
    }
}
