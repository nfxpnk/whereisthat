## Context

`FileScanner::ScanFolder()` currently enumerates physical directories with Win32, inserts ordinary folder and file rows inside the staged scan transaction, and finalizes each folder's stored recursive `content_size` bottom-up. `ScanCoordinator` publishes the resulting file/folder totals and latest `disk_scan_statistics` only after a successful staged scan. Browsing, searching, sorting foundations, and offline navigation already depend on normalized `folders`/`files` ownership rather than the source filesystem.

The executable already compiles against and deploys libarchive, but no scanner code calls it. The Add New Disk/Media dialog also displays `Browse inside compressed files (ZIP, ARJ, RAR, 7Z...)`; its current specification deliberately leaves that option inert. This change turns that one option into scan input while retaining the current new-format-only database policy.

## Goals / Non-Goals

**Goals:**

- Scan readable physical archive files through libarchive as part of an enabled ordinary media scan.
- Represent each successfully expanded archive in `folders`, distinguish it from a physical directory, and store archive descendants in the existing hierarchy.
- Preserve the browser's current folder ownership, paging, stored-size, offline navigation, and search paths for archive content.
- Continue scanning after archive-specific failures and record latest successful archive counts.
- Keep failed archive probing atomic with respect to virtual rows and compatible with scan cancellation.

**Non-Goals:**

- Introduce an `archives` table, archive-only navigation model, extraction UI, or live access to archived data after scanning.
- Expand archives embedded as entries inside another archive in the first implementation; such entries are stored as archive-contained files.
- Implement the deferred archive description-import or unchanged-archive update options.
- Preserve or upgrade catalog databases created before the new required archive fields.

## Decisions

- Activate archive scanning only when the existing `Browse inside compressed files` checkbox is accepted in the media dialog. Add a `browseArchives` result/scan option passed through `ScanCoordinator` to `FileScanner`; when it is false, all physical non-directories retain current file behavior. This provides user control for scan cost and leaves default scans compatible. Alternative considered: attempt libarchive expansion on every scan automatically; rejected because the existing UI already promises an explicit archive-browsing choice and probing files increases scan time.

- Extend `folders` with `entry_type TEXT NOT NULL DEFAULT 'directory' CHECK (entry_type IN ('directory','archive'))`. Physical root and directory rows use `directory`; the folder-like row representing a successfully expanded physical archive uses `archive`; directories inside it use `directory`. Extend `FolderEntry` and browser/search display projections so archive-backed folders remain navigable as directories while presentation can identify them. Alternative considered: infer archives from names/attributes; rejected because filename extensions and Win32 archive attributes do not establish virtual-folder identity.

- Do not introduce a container file row for a successfully expanded archive. The archive's physical path becomes the `path` of its archive-backed folder row under the containing physical folder, and archive child paths extend that stored path. If expansion is disabled or fails, only the existing ordinary file insertion is used. Alternative considered: persist both a file and folder with the same displayed archive name; rejected because it duplicates one source item in its containing view and makes counts/navigation ambiguous.

- Probe physical non-directory items with libarchive only on the archive-enabled path. Configure libarchive's supported formats and filters, open the source file, iterate headers, and consume entry data sufficiently to surface read, encryption, truncation, and corruption failures; when CRC is enabled, calculate stored CRC-32 while consuming each contained regular file's uncompressed bytes. An empty archive that opens and reaches clean end-of-archive is a readable archive folder with no children. Alternative considered: identify candidates by extension alone or treat header discovery as success; rejected because supported archives may have unusual names and a header-only pass can publish content that cannot be read.

- Make expansion failure local to the candidate archive. Insert tentative archive-backed rows and their descendants within an archive-local SQLite savepoint, or an equivalently bounded staging structure, and release them only after libarchive completes cleanly. On unsupported format, encrypted data, warning escalated as unreadable, corruption, or other read failure, roll back tentative rows, log a nonfatal diagnostic including the physical archive path and libarchive reason, then insert the physical file using the established file/CRC path. Cancellation remains a cancellation of the outer scan rather than a fallback file result. Alternative considered: fail the entire disk scan for one unreadable archive; rejected because unreadable files must not prevent indexing unrelated media content.

- Normalize archive entry paths before persistence. Convert archive separators to stored path separators, discard empty and `.` components, reject absolute/rooted paths and any `..` traversal, and ensure every child remains below its archive-backed folder path. For a valid file entry whose parent directories were not explicitly emitted by the archive, create required intermediate `directory` rows once and preserve parent ids. Entries with unsafe/unrepresentable paths cause archive expansion to fall back as unreadable rather than escaping or ambiguously flattening hierarchy. Alternative considered: concatenate libarchive names directly; rejected because archive member paths are untrusted and SQLite's unique folder-path invariant requires canonical hierarchy.

- Reuse stored aggregate behavior for archives. `content_size` for an archive-backed folder is the sum of persisted uncompressed file sizes in its archived subtree; physical ancestor folder totals include that stored subtree once. On successful expansion, `FileScanner::Result.totalFolders` includes the archive folder and internal folder rows and `totalFiles` includes internal file rows instead of the container file. Alternative considered: retain the compressed container byte size in folder totals; rejected because existing folder totals describe browseable stored descendant files.

- Add latest-scan statistic fields `scanned_archives`, `archive_files_count`, and `archive_folders_count` as required non-negative integer columns in `disk_scan_statistics`, with matching fields in scan results/statistics models. `scanned_archives` counts readable archive-backed folder rows; `archive_files_count` counts contained file rows; `archive_folders_count` counts directory rows contained below archives, including synthesized directories but excluding each archive-backed container already represented by `scanned_archives`. Alternative considered: calculate archive counts on demand by walking paths; rejected because file rows do not otherwise need archive-origin flags and the requirement concerns latest scan results.

- Treat `entry_type` and archive statistic fields as required replacement-format schema data. `InitializeSchema()` creates them, `HasCatalogSchema()` validates them, and maintained database/spec documentation describes their semantics. Existing catalogs missing these fields are rejected without mutation, consistent with the established handling of catalogs missing `folders.content_size`. Alternative considered: add `ALTER TABLE` migration defaults; rejected because the current storage contract explicitly supports no format migrations.

## Risks / Trade-offs

- [Archive-enabled scans may take substantially longer because members are read and optional CRCs cover uncompressed bytes] -> Keep the option explicit, report normal scan progress/counts including archived entries, and retain cooperative cancellation during libarchive reads.
- [Archive formats can omit directory entries or provide hostile paths] -> Canonicalize and validate member paths and create only safe required parents beneath the archive folder.
- [A damaged archive may fail after many entries have been encountered] -> Isolate its tentative writes with an archive-local rollback boundary and account totals only after success.
- [Replacing a readable archive file row removes access to its compressed file size/CRC as a visible item] -> Document that folder `content_size` and file totals describe browsable archived contents; the archive row's purpose is navigation.
- [No general application logging subsystem exists today] -> Introduce a focused nonfatal scanner diagnostic sink usable by tests, with an initial native debug/log output route, rather than turning diagnostics into scan failure.

## Migration Plan

1. Update maintained storage/UI/architecture/acceptance contracts and database documentation to define archive-backed folders, option behavior, statistic meanings, and the required-format break.
2. Extend schema validation/creation, folder/statistics domain models, database inserts and display projections with the new required fields.
3. Carry `browseArchives` from the dialog result through scan orchestration and implement libarchive member scanning, canonical hierarchy creation, failure logging/fallback, CRC handling, stored sizes, and cancellation.
4. Extend smoke coverage with generated readable/corrupt archive fixtures, hierarchy/statistics/navigation queries, option-disabled behavior, and catalogs lacking archive fields; then build and manually verify browsing.

Rollback is source-only: an executable reverted before this change is not required to open catalogs created with required archive fields, matching the existing replacement-format policy.

## Open Questions

None block implementation. Recursive expansion of archive members and any future display icon/column for archive-backed folders require separate product decisions; this capability only requires type information to remain available to the navigation presentation.
