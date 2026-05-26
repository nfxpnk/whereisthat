## Why

Folder rows in the right-pane contents view need meaningful full-folder size values, but calculating recursive totals while an owner-data page is painted makes navigation pause and does not provide a durable value for future database-backed sorting. Folder content totals should be captured once as part of indexed catalog data so offline browsing remains fast and consistently queryable.

## What Changes

- Persist each folder's recursive stored file-byte total as part of the normalized `folders` record.
- Calculate folder content totals during background add/update scanning and publish them only with the successful staged scan transaction.
- Display persisted folder sizes in right-pane contents and include them in focused-item and selected-item size reporting without executing recursive aggregate reads during browsing.
- Keep mixed folder/file right-pane paging database-backed, with size represented by ordinary stored/projected values suitable for later SQL ordering; implementing interactive sort controls remains outside this change.
- Tighten current-location content reads and supporting indexes where needed so opening a disk or folder reads its immediate persisted children rather than paying disk-wide or descendant-aggregate browse cost.
- **BREAKING**: The replacement catalog format gains required stored folder-size data. Catalog databases created with an earlier normalized format that lacks the new field are unsupported; this change adds no conversion, backfill, or migration path.

## Capabilities

### New Capabilities

- `catalog-folder-content-sizes`: Persist scan-time recursive folder byte totals and serve responsive offline folder/file contents pages from stored values, while establishing database-side size data suitable for future sorting.

### Modified Capabilities

None. There are no promoted baseline OpenSpec capability specs; this change extends the maintained normalized storage and browser contracts.

## Impact

- Affected code: `src/core/FolderEntry.h`, `src/core/FileScanner.cpp`, `src/storage/Database.h/.cpp`, `src/ui/FileListView.cpp`, status handling in `src/app/BrowserController.cpp`, and focused smoke verification.
- Affected stored format: `folders` schema, schema validation, query/index definitions, and database documentation in `docs/database/`.
- Affected product documentation: maintained storage/UI/architecture/acceptance specifications must state persisted folder totals, responsive paged browsing, and intentional rejection of catalog files lacking the required field.
- Dependencies and deployment remain unchanged: native C++/Win32/WTL plus the existing SQLite DLL.
