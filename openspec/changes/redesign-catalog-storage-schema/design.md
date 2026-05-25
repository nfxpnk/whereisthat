## Context

The current executable schema is centralized in `src/storage/Database.cpp`. It creates `catalogs` rows that now function as indexed media sources and `files` rows that represent both folders and files, with `parent_path`, ISO-formatted text timestamps, an attributes bitmask, and an `is_directory` discriminator. `src/core/FileScanner.cpp` streams `FileEntry` inserts from Win32 enumeration; `src/app/MainFrame.cpp` stages scans in an in-memory database copy and persists the copy only on Save. Browsing and search depend on paged prepared queries, and the native Add New Disk/Media dialog currently captures a disk-name string plus a disk-number string but does not persist broader metadata or execute its CRC/description options.

The replacement catalog format must store substantially richer catalog, disk, scan, folder, and file metadata. This change is intentionally allowed to reject all old-format catalogs: it shall not add table alteration, legacy data copying, or old-schema validation paths. It must preserve the native C++/Win32/SQLite deployment, active catalog files, asynchronous staging and explicit Save, and offline paged browsing model.

Database documentation in `docs/database/` and its verifier in `tools/database-docs/verify.ps1` currently describe the two-table schema and must move with the new executable schema. Maintained architectural/storage documentation still states that existing database files remain compatible; that contract must be deliberately revised.

## Goals / Non-Goals

**Goals:**

- Define a normalized SQLite format for one catalog metadata row, many disks, normalized folder hierarchy, file records, and the latest scan statistics per disk.
- Populate disk and file metadata from native Windows data where it is obtainable, and store all persisted dates/times as Unix timestamps in `INTEGER` columns.
- Keep source replacement, staged editing, Save, paging, navigation, and search functional against the new tables.
- Compute catalog summary totals from normalized data and read the physical catalog file size from the active database file on disk.
- Provide typed core models and storage operations that make disk types, optional CRCs, and Boolean/flag semantics explicit.
- Replace database documentation and update the verification tool alongside the executable schema.

**Non-Goals:**

- Opening, converting, migrating, or diagnosing catalog files that use the current `catalogs`/`files` format.
- Persisting catalog file size or catalog-wide cached totals.
- Adding a history of multiple scans per disk; only latest scan statistics are required.
- Implementing a general description-import subsystem beyond recording descriptions/counts produced by implemented scan inputs.
- Guessing a disk medium/type value that Windows metadata or accepted user input cannot establish reliably.

## Decisions

- Replace the existing schema outright with these application tables:

  | Table | Responsibility |
  | --- | --- |
  | `catalog_metadata` | Exactly one row for catalog-owned metadata, initially `description`; use a fixed key such as `id = 1`. |
  | `disks` | One row per added disk/media source, including stable source path for update identity and all requested disk metadata. |
  | `disk_scan_statistics` | One-to-one latest scan-statistics row keyed by `disk_id`. |
  | `folders` | Folder hierarchy owned by a disk, with optional parent-folder reference and retained filesystem metadata needed for offline browse display. |
  | `files` | File-only metadata owned by a disk and containing folder, with description/CRC/timestamps/attributes. |

  Alternative considered: extend `catalogs` and keep folder rows in `files`. Rejected because the schema is explicitly allowed to break compatibility and separate disk/folder/file concepts avoid overloaded naming and Boolean item-type predicates.

- Model relationships explicitly with foreign keys and cascading deletion: `folders.disk_id -> disks.id`, `folders.parent_folder_id -> folders.id`, `files.disk_id -> disks.id`, `files.folder_id -> folders.id`, and `disk_scan_statistics.disk_id -> disks.id`. Retain a normalized source-path column on `disks` for Add/Update identity, with case-insensitive indexed uniqueness inside one catalog so replacement does not rely on duplicate cleanup. Alternative considered: continue deleting duplicate path matches at runtime; rejected because a new schema can encode the invariant directly.

- Store disk fields on `disks`: `disk_name`, `disk_number`, `source_path`, `volume_label`, `total_capacity`, `free_space`, `cluster_size`, `serial_number`, `file_system`, `total_files`, `total_folders`, `added_at`, `updated_at`, `description`, `category`, `location`, and `disk_type`. Per-disk `total_files` and `total_folders` are stored because the requested disk record includes scan totals; catalog summary queries shall use normalized `files` and `folders` counts rather than sum these cached scan outcomes. Alternative considered: omit the disk totals as derivable; rejected because they are explicitly required disk information and provide the outcome captured for that disk scan.

- Store only the latest scan outcome in a separate `disk_scan_statistics` row containing `last_scanned_at`, `image_scanning_time_ms`, `imported_descriptions_count`, and `calculated_file_crcs` (`0`/`1`). A separate one-to-one table keeps operational scan metadata distinguishable from identity/media properties without creating unsupported history. Alternative considered: place the values on `disks`; valid but less clear as scan processing expands.

- Represent file attributes using the existing Win32-style bitmask approach rather than five duplicated Boolean columns: persist `attributes INTEGER NOT NULL DEFAULT 0`, document the `FILE_ATTRIBUTE_HIDDEN`, `SYSTEM`, `READONLY`, `COMPRESSED`, and `ARCHIVE` flags, and expose helper predicates where UI/domain code needs individual values. Alternative considered: five `INTEGER` columns; rejected because the scanner already receives a Win32 mask and preserving it retains additional native flags without schema churn.

- Store `folders` separately with `id`, `disk_id`, nullable `parent_folder_id`, `name`, a persisted path value adequate for navigation/source traceability, `created_at`, `modified_at`, `accessed_at`, and `attributes`. Although the immediate request requires folder counting, the existing UI already browses and displays folder metadata offline; retaining parallel timestamps and attributes avoids losing existing behavior when folders no longer occupy `files`. Alternative considered: store only names and parent IDs; rejected because display metadata and traceable offline paths would be lost.

- Store `files` with `id`, `disk_id`, `folder_id`, `name`, `description`, `extension`, nullable `crc`, `size`, `created_at`, `modified_at`, `accessed_at`, and `attributes`. `extension` is stored without the leading dot and uses empty text for no extension; `crc` is `NULL` when no CRC is calculated or no value is available. Alternative considered: empty text for an absent CRC; rejected because `NULL` distinguishes unavailable/not-calculated data from a present textual value.

- Use Unix timestamp seconds in all persisted timestamp fields (`added_at`, `updated_at`, `last_scanned_at`, file/folder `created_at`, `modified_at`, and `accessed_at`). Extend native time helpers to convert `FILETIME` and current UTC system time to `std::int64_t` Unix values, converting to formatted display text only at the UI boundary. Alternative considered: retain ISO text; rejected by the required database format.

- Define `DiskType` as a C++ enum serialized to exactly one of `CD`, `DVD`, `BluRay`, `HardDisk`, `SolidStateDisk`, `RemovableUSB`, `VirtualDisk`, or `Other`, with a SQL `CHECK` constraint. Use Windows device/volume data and selected source kind only where classification is reliable: for example mounted virtual image sources can be `VirtualDisk`, removable sources can be `RemovableUSB`, and fixed storage may be classified with supported storage-property queries. If the implementation cannot reliably distinguish a requested subtype such as optical media format, it stores `Other` unless an explicit accepted UI value is introduced. Alternative considered: infer a precise type from insufficient metadata; rejected because it would fabricate persisted information.

- Gather disk metadata through focused native platform/scanner helpers, using applicable APIs such as `GetVolumeInformationW` and `GetDiskFreeSpaceExW` / `GetDiskFreeSpaceW`, tolerating unavailable values with documented defaults or nullable fields. The scan request passed from the Add New Disk/Media dialog must carry the numeric disk number, accepted disk name, source identity, disk type, and enabled scan options needed by storage. Alternative considered: collect metadata in SQLite code; rejected because storage should persist values rather than own Win32 discovery.

- Wire CRC calculation through the existing presented scan option when made functional by this change. Persist standard CRC-32 as consistently formatted text for files successfully processed while the option is enabled; record `calculated_file_crcs = 1` for a scan performed with CRC calculation enabled and leave file `crc` null when disabled or unavailable. Imported description counts remain `0` unless an implemented importer supplies descriptions during that scan; stored description fields make subsequent behavior possible without claiming an importer already exists. Alternative considered: generate CRC values regardless of option; rejected because scanning cost and the UI option would be misleading.

- Provide storage summary queries returning disk count from `disks`, total files from `files`, total folders from `folders`, total storage capacity from `SUM(disks.total_capacity)`, and total used space from `SUM(disks.total_capacity - disks.free_space)`. Obtain current catalog file size from its filesystem path via a platform helper, not SQL. Alternative considered: a catalog summary row; rejected because those values can become stale and the application has no deliberate catalog aggregate cache.

- Replace existing schema validation with validation of only the new-format schema and required objects, optionally using a newly defined schema marker/value for the new format if useful for clear rejection. Do not retain `ALTER TABLE files ADD COLUMN is_directory`, legacy `catalogs` probes, or copy logic for old objects. Staged Save copies/replaces the new table set transactionally in dependency order. Alternative considered: recognize both formats; rejected by the no-compatibility requirement.

- Index the new relationships and implemented access paths: unique case-insensitive disk source path, disk number as appropriate for lookup/presentation, folder `(disk_id, parent_folder_id)` hierarchy navigation, file `folder_id`/`disk_id` scoping, file extension, and file name/search paths. Add a CRC index only when duplicate/CRC lookup is actually wired during implementation; documenting a speculative performance index is not necessary. Alternative considered: carry forward indexes whose consumers do not exist; rejected in favor of evidenced query support.

## Risks / Trade-offs

- [Existing user databases become unusable after the upgrade] -> Make the breaking format explicit in UI/documentation/acceptance verification and validate only newly created replacement-format databases; do not partially open legacy files.
- [Staged Save currently copies only two tables and could omit or violate foreign-key order for the new graph] -> Replace copy/delete operations together and test Save/Discard with disks, folders, files, metadata, and scan statistics.
- [Splitting folder rows from files affects tree, page, and search queries] -> Implement folder/file union or coordinated page queries in storage only, preserving offline behavior and owner-data UI paging.
- [Windows cannot always identify requested disk classifications, especially optical media subtype] -> Persist only a reliably established enum value and use `Other` for unavailable classification rather than inventing values.
- [CRC computation can materially slow large scans] -> Perform it only when the option is enabled, retain asynchronous scanning/progress handling, and record the option/result in latest statistics.
- [Per-disk scan counts can diverge if writes are incomplete] -> Update disk totals and latest statistics only as part of the successful staged scan transaction; calculate catalog totals independently from normalized child rows.
- [Changing timestamp representation affects all displays and searches] -> centralize Unix timestamp conversion in platform helpers and update UI formatting and database documentation together.
- [The existing documentation verifier is coupled to C++ SQL literal extraction] -> Update it alongside the canonical initializer and table documentation, verifying new table and column coverage rather than legacy migration artifacts.

## Migration Plan

1. Amend canonical `docs/spec/` storage/architecture/UI/acceptance documentation to state that the next format is new-catalog-only, normalized, Unix-timestamp based, and not backward compatible.
2. Introduce replacement core models and platform metadata/time helpers, including disk type and summary structures, before switching storage queries.
3. Replace executable schema initialization/validation and staged-save persistence with the new tables, foreign keys, and evidenced indexes; remove old-format upgrade/validation behavior.
4. Adapt scanner and Add/Update orchestration to capture disk metadata, folders, file metadata, optional CRCs, and latest statistics transactionally.
5. Adapt browser/search/status/catalog-summary access to the normalized queries and filesystem-read catalog file size.
6. Replace `docs/database/` SQL/Markdown inventory and update `tools/database-docs` verification for the new schema.
7. Build and validate fresh database creation, metadata round-trips, computed summaries, staging/Save, paging/search, and intentional legacy rejection.

Rollback is a code rollback only. Catalog files created in the replacement format are not guaranteed readable by older application builds, and old-format database files are not converted by this change.

## Open Questions

- No schema decision blocks implementation. Exact Windows device-query coverage for distinguishing `CD`, `DVD`, `BluRay`, `HardDisk`, and `SolidStateDisk` should be documented during implementation; any unresolved classification must fall back to `Other`.
- Standard CRC-32 hexadecimal text is selected for newly implemented CRC calculation; if the product later requires interoperability with an existing alternate CRC representation, that will require an explicit follow-up contract.
