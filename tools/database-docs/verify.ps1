[CmdletBinding()]
param(
    [string]$SourceFile = "src/storage/Database.cpp",
    [string]$DocsRoot = "docs/database"
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

if (-not (Test-Path -LiteralPath $SourceFile)) {
    throw "Schema source file not found: $SourceFile"
}
if (-not (Test-Path -LiteralPath $DocsRoot)) {
    throw "Database documentation directory not found: $DocsRoot"
}

$source = Get-Content -LiteralPath $SourceFile -Raw
$tablePattern = 'Exec\("(?<statement>CREATE TABLE IF NOT EXISTS (?<name>[A-Za-z_][A-Za-z0-9_]*) \((?<body>[^"]+)\);)"\)'
$sourceTables = [regex]::Matches($source, $tablePattern)
if ($sourceTables.Count -eq 0) {
    Fail "No CREATE TABLE statements were extracted from $SourceFile."
}

foreach ($table in $sourceTables) {
    $name = $table.Groups["name"].Value
    $sourceStatement = Normalize-Sql $table.Groups["statement"].Value
    $sourceColumns = Columns-FromBody $table.Groups["body"].Value
    $sqlFile = Join-Path $DocsRoot "schema/tables/$name.sql"
    $mdFile = Join-Path $DocsRoot "tables/$name.md"

    if (-not (Test-Path -LiteralPath $sqlFile)) {
        Fail "Missing table SQL file: $sqlFile"
    } else {
        $documentedSql = Get-Content -LiteralPath $sqlFile -Raw
        $ddlMatch = [regex]::Match($documentedSql,
            "(?is)CREATE\s+TABLE\s+IF\s+NOT\s+EXISTS\s+$name\s*\((?<body>.*?)\)\s*;")
        if (-not $ddlMatch.Success) {
            Fail "No CREATE TABLE statement for '$name' found in $sqlFile."
        } else {
            $docStatement = Normalize-Sql $ddlMatch.Value
            if ($docStatement -ne $sourceStatement) {
                Fail "Documented CREATE TABLE for '$name' differs from the C++ initializer."
            }
            $docColumns = Columns-FromBody $ddlMatch.Groups["body"].Value
            if (Compare-Object $sourceColumns $docColumns) {
                Fail "Column list in $sqlFile differs from the C++ initializer."
            }
        }
    }

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
        }
    }
}

$knownSqlFiles = Get-ChildItem -LiteralPath (Join-Path $DocsRoot "schema/tables") -Filter "*.sql" -ErrorAction SilentlyContinue
foreach ($file in $knownSqlFiles) {
    if ($file.BaseName -notin @($sourceTables | ForEach-Object { $_.Groups["name"].Value })) {
        Fail "Documented table SQL has no matching initializer table: $($file.FullName)."
    }
}

$sharedChecks = @(
    @{ Pattern = 'CREATE (?:UNIQUE )?INDEX IF NOT EXISTS [^"]+;'; File = "schema/indexes.sql"; Label = "index" },
    @{ Pattern = 'PRAGMA (foreign_keys|journal_mode|synchronous)=[^"]+;'; File = "schema/pragmas.sql"; Label = "configuration PRAGMA" }
)
foreach ($check in $sharedChecks) {
    $documentPath = Join-Path $DocsRoot $check.File
    if (-not (Test-Path -LiteralPath $documentPath)) {
        Fail "Missing shared SQL file: $documentPath"
        continue
    }
    $documentText = Normalize-Sql (Get-Content -LiteralPath $documentPath -Raw)
    foreach ($statement in [regex]::Matches($source, $check.Pattern)) {
        if ($documentText -notlike "*$(Normalize-Sql $statement.Value)*") {
            Fail "Missing documented $($check.Label) statement in $documentPath`: $($statement.Value)"
        }
    }
}

foreach ($emptyObjectFile in @("schema/triggers.sql", "schema/views.sql")) {
    if (-not (Test-Path -LiteralPath (Join-Path $DocsRoot $emptyObjectFile))) {
        Fail "Missing empty-object inventory file: $emptyObjectFile"
    }
}
if ($source -match '(?i)ALTER\s+TABLE\s+') {
    Fail "Legacy-style ALTER TABLE SQL exists; the replacement catalog format must not define migrations."
}
if ($source -match '(?i)CREATE\s+(TRIGGER|VIEW)\s+') {
    Fail "Application-defined trigger or view SQL exists; extend this verifier and documentation inventory."
}

if ($failures.Count -gt 0) {
    Write-Error ("Database documentation verification failed:`n - " + ($failures -join "`n - "))
    exit 1
}

Write-Output "Database documentation verified: $($sourceTables.Count) tables and their documented fields match $SourceFile."
Write-Output "Checked table SQL, shared index/PRAGMA SQL, absence of migration DDL, overview coverage, and schema inventory coverage."
