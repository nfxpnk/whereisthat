## 1. Contract And Format Documentation

- [x] 1.1 Update `docs/spec/storage.md`, `docs/spec/architecture.md`, `docs/spec/ui.md`, and `docs/spec/acceptance.md` to define the new-catalog-only normalized format, Unix timestamp storage, derived catalog summaries, and intentional rejection of old-format catalogs.
- [x] 1.2 Review active OpenSpec change documentation that mentions compatibility with `catalogs`/mixed `files` and record or update the superseding replacement-format assumption where needed to keep implementation guidance coherent.

## 2. Core Models And Native Metadata Helpers

- [x] 2.1 Replace or extend core storage-facing models with explicit catalog metadata, disk, latest scan statistics, folder, file, disk type, scan options, and catalog summary representations needed by the replacement schema.
- [x] 2.2 Add native timestamp conversion helpers that write/read Unix timestamp seconds for current time and Win32 file times, with presentation formatting kept outside SQLite.
- [x] 2.3 Add focused Windows media metadata discovery for available volume label, capacity, free space, cluster size, serial number, filesystem, source identity/location, and reliably classifiable disk type, falling back to `Other` where subtype cannot be established safely.
- [x] 2.4 Change extension normalization so persisted file extensions exclude the leading dot and verify filenames without extensions produce empty text.

## 3. Replacement SQLite Schema And Data Access

- [x] 3.1 Replace `Database::InitializeSchema()` and validation with fresh-format-only creation/validation for `catalog_metadata`, `disks`, `disk_scan_statistics`, `folders`, and file-only `files`, including foreign keys, disk-type/Boolean constraints, and indexes required by implemented lookups and paging.
- [x] 3.2 Remove legacy schema-upgrade and old-format compatibility paths, including `catalogs` validation and `is_directory` alteration behavior, so old catalog files are rejected without modification.
- [x] 3.3 Implement prepared storage operations for catalog description, disk add/update/identity lookup, latest statistics, folder insertion, file insertion, and cascade-safe disk content replacement.
- [x] 3.4 Update staged working-copy Save persistence to copy and replace all replacement-format tables transactionally in foreign-key-safe order while retaining Save/Discard behavior.
- [x] 3.5 Add catalog summary reads that calculate disk/file/folder counts and capacity/used-space totals from normalized tables, plus a filesystem-based current catalog file-size read that is never stored in SQLite.

## 4. Scanning And Add/Update Population

- [x] 4.1 Extend the Add New Disk/Media result and Add/Update orchestration to pass validated numeric disk number, disk name, source identity, metadata/classification, and applicable scan options into staged scanning.
- [x] 4.2 Refactor `FileScanner` to insert normalized folders and files for a disk, populate Unix created/modified/accessed timestamps and native attribute masks, and maintain successful per-disk file/folder totals.
- [x] 4.3 Populate disk volume/capacity/filesystem metadata and `added_at`/`updated_at` values on add or rescan while preserving initial added time and replacing only the matched disk's staged content.
- [x] 4.4 Implement optional CRC-32 calculation when enabled, persist nullable file CRC text consistently, and write latest scan statistics including elapsed milliseconds, CRC Boolean, last-scanned time, and implemented imported-description count.

## 5. Browsing, Search, And Presentation Integration

- [x] 5.1 Adapt catalog tree and owner-data list storage queries to read disks, folders, and files from normalized relationships with paging and offline browsing behavior preserved.
- [x] 5.2 Adapt item search result queries and domain/UI projection to file-only and folder data as appropriate, displaying Unix-timestamp-backed metadata correctly.
- [x] 5.3 Integrate catalog summary access and any currently exposed metadata views/status surfaces without introducing persisted catalog aggregate totals.

## 6. Database Documentation And Tooling

- [x] 6.1 Replace `docs/database/schema/` SQL documentation with one formatted SQL definition per replacement table plus the applicable index, PRAGMA, trigger, and view inventories, explicitly omitting legacy migration SQL.
- [x] 6.2 Replace `docs/database/tables/`, `docs/database/README.md`, and `docs/database/schema-inventory.md` with complete per-field traceability for metadata, disks, scan statistics, folders, and files, including derived values, disk type values, Unix timestamps, Boolean/attribute semantics, optional CRC behavior, and the unsupported-old-format policy.
- [x] 6.3 Update `tools/database-docs/verify.ps1` and its README to validate the replacement table/field/DDL inventory and no longer require or describe legacy additive migration behavior.
- [x] 6.4 Update `docs/ARCHITECTURE.md`, root project documentation, and any other maintained developer notes that still describe the former stored schema or timestamp representation.

## 7. Validation

- [x] 7.1 Run the database documentation verification tool against the implemented replacement schema and confirm every new table/field/index is documented with no legacy-object expectation.
- [x] 7.2 Add focused available automated validation or a repeatable storage smoke check for fresh catalog creation, old-format rejection without mutation, catalog description persistence, disk/file/folder/statistics round-trips, normalized extension and attribute persistence, and nullable/enabled CRC behavior.
- [x] 7.3 Validate computed summaries and physical catalog file-size retrieval, confirming no catalog file size or catalog-wide derived totals are stored in the schema.
- [x] 7.4 Smoke test staged Add/Update, Save/Discard, browser hierarchy, paging, and item search with the normalized records, including a disk rescan that preserves `added_at` and advances `updated_at`.
- [x] 7.5 Build `whereisthat.sln` for `Release|x64` with MSBuild and confirm native Win32/SQLite DLL deployment remains intact.
