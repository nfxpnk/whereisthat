[CmdletBinding()]
param(
    [string]$SqlRoot = "sql",
    [string]$DocsRoot = "docs/database",
    [string]$CatalogSchemaSource = "src/modules/wit_database/src/CatalogSchema.cpp",
    [string]$ImportScript = "tools/import/import.py"
)

$ErrorActionPreference = "Stop"
$failures = [System.Collections.Generic.List[string]]::new()

function Fail([string]$message) {
    $script:failures.Add($message)
}

function Normalize-Sql([string]$sql) {
    return (($sql -replace "--[^\r\n]*", "") -replace "\s+", " " -replace "\s*=\s*", "=" `
        -replace ",\s*", "," -replace "\(\s*", "(" -replace "\s*\)", ")").Trim()
}

function Columns-FromBody([string]$body) {
    $parts = [System.Collections.Generic.List[string]]::new()
    $start = 0
    $depth = 0
    $quoted = $false
    for ($index = 0; $index -lt $body.Length; $index++) {
        $character = $body[$index]
        if ($character -eq "'") {
            $quoted = -not $quoted
        } elseif (-not $quoted -and $character -eq "(") {
            $depth++
        } elseif (-not $quoted -and $character -eq ")") {
            $depth--
        } elseif (-not $quoted -and $character -eq "," -and $depth -eq 0) {
            $parts.Add($body.Substring($start, $index - $start))
            $start = $index + 1
        }
    }
    $parts.Add($body.Substring($start))
    $columns = @()
    foreach ($part in $parts) {
        $definition = $part.Trim()
        if ($definition -match "^(CONSTRAINT|FOREIGN|PRIMARY|UNIQUE|CHECK)\b") {
            continue
        }
        if ($definition -match "^([A-Za-z_][A-Za-z0-9_]*)\s+") {
            $columns += $Matches[1]
        }
    }
    return $columns
}

if (-not (Test-Path -LiteralPath $SqlRoot)) {
    throw "Schema SQL directory not found: $SqlRoot"
}
if (-not (Test-Path -LiteralPath $DocsRoot)) {
    throw "Database documentation directory not found: $DocsRoot"
}

$tableFiles = Get-ChildItem -LiteralPath (Join-Path $SqlRoot "tables") -Filter "*.sql" -ErrorAction SilentlyContinue |
    Sort-Object Name
if ($tableFiles.Count -eq 0) {
    Fail "No table SQL files were found under $SqlRoot/tables."
}

foreach ($sqlFileInfo in $tableFiles) {
    $name = $sqlFileInfo.BaseName
    $sqlFile = $sqlFileInfo.FullName
    $mdFile = Join-Path $DocsRoot "tables/$name.md"
    $sql = Get-Content -LiteralPath $sqlFile -Raw
    $ddlMatch = [regex]::Match($sql,
        "(?is)CREATE\s+TABLE\s+IF\s+NOT\s+EXISTS\s+$name\s*\((?<body>.*?)\)\s*;")

    if (-not $ddlMatch.Success) {
        Fail "No CREATE TABLE statement for '$name' found in $sqlFile."
        continue
    }

    $normalizedFileSql = Normalize-Sql $sql
    $normalizedDdl = Normalize-Sql $ddlMatch.Value
    if ($normalizedFileSql -ne $normalizedDdl) {
        Fail "$sqlFile should contain only the CREATE TABLE statement for '$name'."
    }

    $sourceColumns = Columns-FromBody $ddlMatch.Groups["body"].Value
    if (-not (Test-Path -LiteralPath $mdFile)) {
        Fail "Missing table Markdown file: $mdFile"
    } else {
        $markdown = Get-Content -LiteralPath $mdFile -Raw
        $fieldReference = [regex]::Match($markdown, '(?is)## Field Reference\s*(?<body>.*)$')
        if (-not $fieldReference.Success) {
            Fail "$mdFile has no Field Reference section."
            continue
        }
        $documentedColumns = [regex]::Matches($fieldReference.Groups["body"].Value,
            '(?m)^\|\s*`(?<column>[A-Za-z_][A-Za-z0-9_]*)`\s*\|') |
            ForEach-Object { $_.Groups["column"].Value } |
            Select-Object -Unique
        $missing = @($sourceColumns | Where-Object { $_ -notin $documentedColumns })
        $extra = @($documentedColumns | Where-Object { $_ -notin $sourceColumns })
        if ($missing.Count -gt 0) {
            Fail "$mdFile does not document field(s): $($missing -join ', ')."
        }
        if ($extra.Count -gt 0) {
            Fail "$mdFile documents field(s) absent from schema: $($extra -join ', ')."
        }
    }

    foreach ($summaryFile in @((Join-Path $DocsRoot "README.md"), (Join-Path $DocsRoot "schema-inventory.md"))) {
        if (-not (Test-Path -LiteralPath $summaryFile)) {
            Fail "Missing summary file: $summaryFile"
        } elseif ((Get-Content -LiteralPath $summaryFile -Raw) -notmatch [regex]::Escape("``$name``")) {
            Fail "$summaryFile has no entry for table '$name'."
        } elseif ((Get-Content -LiteralPath $summaryFile -Raw) -notmatch [regex]::Escape("../../sql/tables/$name.sql")) {
            Fail "$summaryFile does not link table '$name' to the root sql directory."
        }
    }
}

foreach ($sharedSqlFile in @("indexes.sql", "pragmas.sql")) {
    $path = Join-Path $SqlRoot $sharedSqlFile
    if (-not (Test-Path -LiteralPath $path)) {
        Fail "Missing shared SQL file: $path"
    } elseif ([string]::IsNullOrWhiteSpace((Get-Content -LiteralPath $path -Raw))) {
        Fail "Shared SQL file is empty: $path"
    }
}

$oldDocsSchema = Join-Path $DocsRoot "schema"
if (Test-Path -LiteralPath $oldDocsSchema) {
    $duplicatedSql = Get-ChildItem -LiteralPath $oldDocsSchema -Filter "*.sql" -Recurse -ErrorAction SilentlyContinue
    if ($duplicatedSql.Count -gt 0) {
        Fail "Documentation schema SQL duplicates the root sql authority: $oldDocsSchema"
    }
}

foreach ($sourcePath in @($CatalogSchemaSource, $ImportScript)) {
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        Fail "Schema consumer source file not found: $sourcePath"
        continue
    }
    $source = Get-Content -LiteralPath $sourcePath -Raw
    if ($source -match '(?i)CREATE\s+(?:UNIQUE\s+)?(?:TABLE|INDEX)\b') {
        Fail "Schema DDL is duplicated in ${sourcePath}; use files under $SqlRoot instead."
    }
    if ($source -match '(?i)ALTER\s+TABLE\s+') {
        Fail "Legacy-style ALTER TABLE SQL exists in ${sourcePath}; the replacement catalog format must not define migrations."
    }
    if ($source -match '(?i)CREATE\s+(TRIGGER|VIEW)\b') {
        Fail "Application-defined trigger or view SQL exists in ${sourcePath}; extend this verifier and documentation inventory."
    }
}

if ($failures.Count -gt 0) {
    Write-Error ("Database documentation verification failed:`n - " + ($failures -join "`n - "))
    exit 1
}

Write-Output "Database documentation verified: $($tableFiles.Count) root SQL table files and their documented fields match."
Write-Output "Checked shared SQL files, absence of duplicated schema DDL, overview coverage, and schema inventory coverage."
