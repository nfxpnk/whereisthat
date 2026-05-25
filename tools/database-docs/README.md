# Database Documentation Verification

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools/database-docs/verify.ps1
```

`verify.ps1` extracts application-defined `CREATE TABLE`, `CREATE INDEX`, and schema-configuration `PRAGMA` statements from `src/storage/Database.cpp`. It checks that:

- every source-defined table has a formatted SQL file and Markdown table page;
- SQL table definitions and shared schema statements remain equivalent to the initializer;
- every schema column is present in its Markdown field table, with no additional documented columns;
- each table appears in `docs/database/README.md` and `docs/database/schema-inventory.md`;
- trigger and view inventory files remain present, and the script fails if application trigger/view DDL is introduced without corresponding extension.
- no `ALTER TABLE` migration statement is introduced for the new-format-only catalog contract.

The script intentionally verifies rather than auto-writes narrative Markdown. Purpose, lifecycle, and field meaning require review against application behavior; generating those claims from column names would risk documenting invented semantics.

For a behavioral check of the native storage API, scanner, and replacement-format rules, run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/storage-smoke/run.ps1
```

The smoke utility compiles a temporary console executable with the same C++ storage/scanner sources and vendored SQLite library. It verifies fresh creation, old-format rejection without schema mutation, metadata and scan-statistics storage, normalized extension/attribute/CRC behavior, derived summaries, and filesystem catalog-size reporting; generated files are confined to the system temporary directory and removed afterward.
