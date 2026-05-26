## Why

The folder tree and content list currently show item names without complete visual type cues, even though the browser presents catalog databases, disk/media entries, folders, files, and navigable archive-backed folders. In addition, fixed media scans currently persist `Other` because fixed HDD/SSD classification is never attempted after metadata collection.

## What Changes

- Add 16x16 Fugue icons before item names in the main browser tree and right-pane disk/content lists.
- Display `database-sql.png` for each catalog-database root item and `drive.png` for each indexed disk/media tree node or catalog-root disk inventory row.
- Display `folder.png` for ordinary browsable folders, `document.png` for non-archive files, and `vise-drawer.png` for archive container files or archive-backed folders that can be entered as folders.
- Vendor the required Fugue PNG assets from `Q:\fugue-icons-3.5.6\icons` with the application resources and load them for native TreeView/ListView image lists.
- Classify fixed scan volumes as `SolidStateDisk` or `HardDisk` when the native Windows seek-penalty storage property reliably distinguishes them; retain `Other` where reliable classification is unavailable.
- Keep icon selection presentation-only and retain the existing catalog schema; media-type correction changes newly scanned or rescanned persisted metadata only.

## Capabilities

### New Capabilities

- `browser-item-icons`: Native tree/list icon presentation for catalog roots, indexed disk/media entries, stored folders, non-archive files, archive container files, and archive-backed folders in the catalog browser.
- `fixed-drive-media-classification`: Reliable persisted HDD/SSD classification for newly scanned or rescanned fixed volumes.

### Modified Capabilities

None. There are no promoted baseline OpenSpec capability specs; maintained UI documentation is updated during implementation.

## Impact

- Affected native UI code: `src/app/MainWindowChrome.*`, `src/ui/CatalogTreeView.*`, and `src/ui/FileListView.*` for shared icon-list creation, attachment, and per-item image selection.
- Affected application resources and project inputs: `src/app/resource.h`, `src/app/app.rc`, `WhereIsThat.vcxproj`, and vendored Fugue icon assets under `src/app/toolbar/fugue/` or an equivalent existing resource asset location.
- Affected platform/storage metadata path: `src/platform/VolumeInfo.*` and scan verification for fixed-drive `disk_type`; no schema change is required because the allowed values already exist.
- Affected maintained contract: `docs/spec/ui.md`, `docs/spec/storage.md`, database documentation, and acceptance/manual verification.
- Archive-backed rows already expose type information and ordinary file rows expose display metadata suitable for classifying archive files.
