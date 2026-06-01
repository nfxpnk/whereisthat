# Database Documentation Verification

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools/database-docs/verify.ps1
```

`verify.ps1` treats the root `sql` directory as the schema authority. It checks that:

- every source-defined table has a Markdown table page;
- table SQL files match the documented field tables;
- every schema column is present in its Markdown field table, with no additional documented columns;
- each table appears in `docs/database/README.md` and `docs/database/schema-inventory.md`;
- schema DDL is not duplicated back into the C++ initializer;
- no `ALTER TABLE` migration statement is introduced for the new-format-only catalog contract, including required stored folder-size and archive-aware data.

The script intentionally verifies rather than auto-writes narrative Markdown. Purpose, lifecycle, and field meaning require review against application behavior; generating those claims from column names would risk documenting invented semantics.

For a behavioral check of the native storage API, scanner, and replacement-format rules, run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/storage-smoke/run.ps1
```

The smoke utility compiles a temporary console executable with the same C++ storage/scanner sources and vendored SQLite and libarchive libraries. It verifies fresh creation, unsupported-format rejection without schema mutation, metadata and scan-statistics storage, persisted recursive folder sizes, readable archive hierarchy/counts, unreadable archive fallback, normalized extension/attribute/CRC behavior, derived summaries, and filesystem catalog-size reporting; generated files are confined to the system temporary directory and removed afterward.
