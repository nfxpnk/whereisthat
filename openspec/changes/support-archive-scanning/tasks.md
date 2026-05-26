## 1. Contract And Required Catalog Format

- [x] 1.1 Update `docs/spec/` storage, architecture, UI, and acceptance contracts so archive browsing is an implemented opt-in scan behavior using archive-backed folders, shared navigation, latest archive counts, graceful fallback, and no catalog migration.
- [x] 1.2 Extend executable schema creation/validation and `docs/database/` definitions for required `folders.entry_type` directory/archive values and required non-negative `disk_scan_statistics` archive-count fields, explicitly keeping the no-`archives`-table design.
- [x] 1.3 Extend folder, scan-result, and latest-statistics domain structures plus prepared database writes/reads to persist the type marker and archive counters and reject normalized catalogs missing those required fields.

## 2. Scan Option And Archive Reader Plumbing

- [x] 2.1 Make the existing `Browse inside compressed files` dialog checkbox populate an accepted `browseArchives` scan option while leaving the deferred archive sub-options inert.
- [x] 2.2 Carry `browseArchives` through `ScanCoordinator` into `FileScanner`, preserving current scan behavior and zero archive counts when the option is off.
- [x] 2.3 Add a focused scanner diagnostic path for nonfatal libarchive failures that identifies the source archive and can be observed by verification without converting an individual archive failure into media-scan failure.

## 3. Archive Hierarchy Persistence

- [x] 3.1 Implement libarchive probing for physical files during enabled scans, including supported filters/formats, clean end-of-archive detection, entry-data reading, CRC-32 calculation for archived files when selected, and cooperative cancellation.
- [x] 3.2 Implement safe archive member path normalization and parent-map construction, rejecting rooted/traversing/unrepresentable member paths and synthesizing missing directory rows once beneath an archive-backed folder.
- [x] 3.3 Persist a completely readable archive as one `entry_type='archive'` folder plus ordinary descendant folder/file rows, and finalize archive and physical ancestor `content_size` values from persisted uncompressed member sizes.
- [x] 3.4 Isolate tentative archive persistence and accounting behind an archive-local rollback boundary so unsupported, encrypted, corrupt, or unreadable candidates log a diagnostic, leave no virtual rows, and fall back to their ordinary physical file row while scanning continues.
- [x] 3.5 Update successful scan accounting so ordinary totals include the chosen folder/file representation and latest statistics count expanded archive containers, contained files, and contained or synthesized folders with the specified semantics.

## 4. Shared Browse And Presentation Behavior

- [x] 4.1 Extend storage display projections and core presentation records with archive-backed type information while continuing to retrieve archive folders and contents through existing folder/file count, page, tree, search, and stored-size query paths.
- [x] 4.2 Provide the native browser presentation enough type information to distinguish an archive-backed folder without changing activation, Back/Forward, offline browsing, database-backed pagination, or existing folder-first behavior.

## 5. Verification

- [x] 5.1 Extend storage/scan smoke fixtures with an enabled readable archive containing nested members and omitted directory entries; verify type marking, hierarchy, uncompressed stored sizes, optional CRC behavior, navigation/search projections, and archive statistics.
- [x] 5.2 Add archive-disabled, empty-archive, corrupt or unreadable archive, unsafe-member-path, and cancellation coverage; verify ordinary-file fallback, logged diagnostics, scan continuation, no partial virtual rows, and correct zero/nonzero statistics.
- [x] 5.3 Add required-format validation coverage for normalized catalogs lacking archive marker/statistics fields and verify rejection without schema alteration or backfill.
- [ ] 5.4 Run the database documentation verification and storage smoke executable, build `whereisthat.sln` for `Release|x64`, and manually confirm the dialog option and offline tree/list entry into a scanned archive.
