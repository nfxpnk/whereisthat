# Database Documentation Verification

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools/database-docs/verify.ps1
```

`verify.ps1` extracts application-defined `CREATE TABLE`, `CREATE INDEX`, schema-configuration `PRAGMA`, and `ALTER TABLE` statements from `src/storage/Database.cpp`. It checks that:

- every source-defined table has a formatted SQL file and Markdown table page;
- SQL table definitions and shared schema statements remain equivalent to the initializer;
- every schema column is present in its Markdown field table, with no additional documented columns;
- each table appears in `docs/database/README.md` and `docs/database/schema-inventory.md`;
- trigger and view inventory files remain present, and the script fails if application trigger/view DDL is introduced without corresponding extension.

The script intentionally verifies rather than auto-writes narrative Markdown. Purpose, lifecycle, and field meaning require review against application behavior; generating those claims from column names would risk documenting invented semantics.

