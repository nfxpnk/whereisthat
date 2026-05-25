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
        "/I`"$(Join-Path $root 'third_party\sqlite')`""
        "/I`"$(Join-Path $root 'third_party\wtl\Include')`""
        "`"$(Join-Path $root 'tools\storage-smoke\StorageSmoke.cpp')`""
        "`"$(Join-Path $root 'src\app\ScanCoordinator.cpp')`""
        "`"$(Join-Path $root 'src\core\FileScanner.cpp')`""
        "`"$(Join-Path $root 'src\platform\PathHelpers.cpp')`""
        "`"$(Join-Path $root 'src\platform\VolumeInfo.cpp')`""
        "`"$(Join-Path $root 'src\platform\Win32Helpers.cpp')`""
        "`"$(Join-Path $root 'src\storage\Database.cpp')`""
        "`"$(Join-Path $root 'src\storage\SQLiteStatement.cpp')`""
        "`"$(Join-Path $root 'third_party\sqlite\sqlite3.lib')`""
        "/Fe:`"$exe`""
    ) | Set-Content -LiteralPath $response -Encoding ASCII

    Push-Location $buildDir
    try {
        $compile = "call `"$vcVars`" >nul && cl.exe @`"$response`""
        & cmd.exe /d /c $compile
        if ($LASTEXITCODE -ne 0) { throw "Storage smoke compilation failed." }
        Copy-Item -LiteralPath (Join-Path $root "third_party\sqlite\sqlite3.dll") -Destination $buildDir
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
