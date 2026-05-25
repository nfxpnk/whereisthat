## Why

The current catalog database stores indexed sources in a legacy-named `catalogs` table and mixes folders and files in a minimal `files` table, so it cannot represent catalog descriptions, complete disk metadata, latest scan statistics, or requested file metadata. The next catalog format can be cleanly normalized because old catalog compatibility and migration are explicitly not required.

## What Changes

- **BREAKING** Replace the existing `catalogs` / mixed-item `files` SQLite contract with a new-catalog-only normalized schema for catalog metadata, disks, folders, files, and current scan statistics.
- Store one catalog description and persisted disk metadata including numeric disk number, volume/filesystem details, capacity/free-space values, descriptive classification, location, disk type, and added/updated timestamps.
- Store the latest scan statistics for each disk, including scan time, last scanned timestamp, imported description count, and whether file CRC calculation was performed.
- Store normalized folder and file records, including file descriptions, optional CRCs, Unix timestamp metadata, and Win32 file attribute flags; store file extensions without a leading dot.
- Use `INTEGER` Unix timestamps and `INTEGER` Boolean/flag values throughout the new persisted model, and define the supported disk type values (`CD`, `DVD`, `BluRay`, `HardDisk`, `SolidStateDisk`, `RemovableUSB`, `VirtualDisk`, `Other`).
- Add storage summary queries for disk/file/folder counts and capacity/used-space totals while reading the catalog database file size from the filesystem rather than persisting it.
- Update scan/staged-save flows and native Windows metadata collection to populate the new schema while retaining SQLite, asynchronous scanning, explicit Save, and paged offline browsing.
- Replace database documentation and its verification tooling to describe and validate the new format; update affected storage, architecture, UI, and acceptance documentation.
- Do not implement a legacy upgrade, schema compatibility branch, or persisted catalog aggregate cache for old-format databases.

## Capabilities

### New Capabilities

- `catalog-storage-schema`: Defines the replacement SQLite catalog data model, metadata capture, computed catalog summaries, new-catalog validation behavior, and documentation/verification contract.

### Modified Capabilities

No archived OpenSpec capability is present to modify. Implementation must update maintained `docs/spec/` contracts and supersede contrary assumptions in active storage-related changes.

## Impact

- Replaces schema creation, validation, prepared SQL, staged-save copying, and aggregate queries in `src/storage/Database.h` and `src/storage/Database.cpp`.
- Reworks domain entities in `src/core` and scanning/metadata collection in `src/core/FileScanner.cpp`, `src/platform`, and Add/Update orchestration in `src/app/MainFrame.cpp`.
- Affects tree/browser/search adapters in `src/ui` because disks, folders, and files become separate stored entities with Unix timestamp display conversion.
- Requires updates to `docs/spec/`, `docs/database/`, `docs/ARCHITECTURE.md`, relevant project documentation, and `tools/database-docs`.
- Invalidates old catalog database files for the updated version by design; existing settings may still point to paths, but only new-format databases are usable.
