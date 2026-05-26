## Why

Archives on scanned media currently remain opaque file rows even though the catalog is intended to support browsing stored contents offline and already ships with libarchive. Users need readable archive contents to participate in the same folder navigation and counting behavior as ordinary scanned directories without introducing a parallel archive hierarchy.

## What Changes

- Make the existing `Browse inside compressed files (ZIP, ARJ, RAR, 7Z...)` scan option functional: when selected, the ordinary background scan attempts to read encountered archive files through the already linked libarchive integration.
- Store each successfully read archive as a folder-like row in `folders`, marked as archive-backed, and store its internal directories and files through the existing `folders` and `files` hierarchy.
- Preserve archive-internal parent/child paths, synthesizing omitted intermediate directory rows when an archive format lists a file without explicit directory entries, so the existing tree/list navigation can enter and browse archives.
- Keep an unsupported, encrypted, corrupt, or otherwise unreadable archive as the ordinary file row it would have been without archive browsing; report the failed archive read without failing the containing media scan.
- Extend latest successful scan reporting with counts for scanned readable archives, files contained in scanned archives, and folders contained inside scanned archives.
- Keep existing browser paging, search, stored folder-size behavior, and catalog totals operating on the normalized folder/file rows; archive-backed rows use those existing paths rather than an `archives` table.
- **BREAKING**: Extend the required replacement catalog format with an archive discriminator on `folders` and archive-count statistics fields. Following the existing new-format-only policy, catalogs lacking the newly required archive fields are rejected rather than migrated or backfilled.

## Capabilities

### New Capabilities

- `archive-backed-folder-scanning`: Opt-in libarchive scanning, archive-backed folder storage and traversal, archive scan statistics, graceful fallback for unreadable archives, and compatibility requirements for the normalized catalog.

### Modified Capabilities

None. There are no promoted baseline OpenSpec capability specs; the maintained product/storage/UI documents are updated as implementation work for the new capability.

## Impact

- Affected scan and option handoff code: `src/ui/AddNewDiskMediaDialog.*`, `src/app/ScanCoordinator.*`, and `src/core/FileScanner.*`.
- Affected domain/storage code: `src/core/FolderEntry.h`, `src/core/Disk.h`, and `src/storage/Database.*` for folder classification, archive statistics, schema validation, and query projections needed to expose type information.
- Affected browsing and display behavior: existing tree/list/search paths continue to consume `folders`/`files`; presentation may distinguish archive-backed folders while navigating them like directories.
- Affected schema and maintained contract: `folders`, `disk_scan_statistics`, database documentation, and `docs/spec/` acceptance/storage/UI/architecture documentation.
- Dependency packaging does not expand: libarchive headers/import library/runtime are already present and linked in the supported MSBuild project.
- Verification expands the existing storage smoke path with readable, nested-path, unreadable/encrypted-or-corrupt fallback, option-disabled, statistics, and schema-validation cases.
