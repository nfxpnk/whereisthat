# `disks`

## Purpose And Lifecycle

Stores one disk/media source added to a catalog. `MainFrame::OnAddOrUpdateDiskImage()` builds the record from accepted dialog input and native volume discovery, then `Database::AddDisk()` creates it or `Database::UpdateDisk()` updates operational metadata after a successful staged scan.

## Keys, Relationships, Indexes And Constraints

- Primary key: `id`.
- Parent for `folders.disk_id`, `files.disk_id`, and `disk_scan_statistics.disk_id`, all cascading on delete.
- `source_path` is unique case-insensitively and stores the root used for normalized browsing. Rescan also matches `location` when an ISO's mounted drive root changes between sessions.
- `disk_type` permits only `CD`, `DVD`, `BluRay`, `HardDisk`, `SolidStateDisk`, `RemovableUSB`, `VirtualDisk`, or `Other`.
- Index: `idx_disks_source_path` on `source_path COLLATE NOCASE`.

## Notes

- `total_files` and `total_folders` are latest successful per-disk scan results. Catalog-wide counts are calculated from `files` and `folders` rather than summing these fields.
- `added_at` is set on insertion and retained during rescans; `updated_at` changes after successful scan completion.
- Volume/capacity fields are read from native Windows APIs for the source's owning volume where available; unknown text/size fields retain their empty/zero defaults. The current scan workflow classifies mounted ISOs as `VirtualDisk`, removable drives as `RemovableUSB`, and otherwise uses `Other` unless a reliable subtype is available.

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description | Used in code | Confidence |
|---|---|---:|---|---|---|---|---|
| `id` | `INTEGER` | No | none | `PRIMARY KEY AUTOINCREMENT` | Disk record identifier. | `src/storage/Database.cpp`; `src/app/MainFrame.cpp`; navigation projections | Explicit |
| `disk_name` | `TEXT` | No | none | none | User-visible disk/media name. | `src/ui/AddNewDiskMediaDialog.cpp`; `src/app/MainFrame.cpp`; `Database::GetCatalogs` | Explicit |
| `disk_number` | `INTEGER` | No | `0` | none | Numeric identifier accepted for the disk. | `src/app/MainFrame.cpp`; `Database::AddDisk`, `UpdateDisk` | Explicit |
| `source_path` | `TEXT` | No | none | `UNIQUE`; `idx_disks_source_path` | Current normalized scan/browse root; normally identifies rescans, and is refreshed if a matched ISO mount root changes. | `MainFrame::OnAddOrUpdateDiskImage`; `Database::FindDiskBySourcePath`, `UpdateDisk`; browser projection | Explicit |
| `volume_label` | `TEXT` | No | `''` | none | Native volume label when available. | `src/platform/VolumeInfo.cpp`; `Database` disk writes | Explicit |
| `total_capacity` | `INTEGER` | No | `0` | none | Total capacity in bytes. | `VolumeInfo.cpp`; `Database::GetCatalogSummary` | Explicit |
| `free_space` | `INTEGER` | No | `0` | none | Free capacity in bytes. | `VolumeInfo.cpp`; `Database::GetCatalogSummary` | Explicit |
| `cluster_size` | `INTEGER` | No | `0` | none | Allocation cluster size in bytes. | `VolumeInfo.cpp`; disk writes | Explicit |
| `serial_number` | `TEXT` | No | `''` | none | Native volume serial number represented as text. | `VolumeInfo.cpp`; disk writes | Explicit |
| `file_system` | `TEXT` | No | `''` | none | Native filesystem name when available. | `VolumeInfo.cpp`; disk writes | Explicit |
| `total_files` | `INTEGER` | No | `0` | none | Latest completed scan file count for this disk. | `FileScanner::Result`; `MainFrame`; `Database::UpdateDisk` | Explicit |
| `total_folders` | `INTEGER` | No | `0` | none | Latest completed scan folder count for this disk. | `FileScanner::Result`; `MainFrame`; `Database::UpdateDisk` | Explicit |
| `added_at` | `INTEGER` | No | none | none | Unix timestamp when first added. | `MainFrame::OnAddOrUpdateDiskImage`; `Database::AddDisk` | Explicit |
| `updated_at` | `INTEGER` | No | none | none | Unix timestamp after the latest completed scan. | `MainFrame::OnAddOrUpdateDiskImage`; root projection | Explicit |
| `description` | `TEXT` | No | `''` | none | Free-text disk description reserved for persisted editing. | `Database::AddDisk`; retained by update behavior | Explicit |
| `category` | `TEXT` | No | `''` | none | Free-text disk category reserved for persisted editing. | `Database::AddDisk`; retained by update behavior | Explicit |
| `location` | `TEXT` | No | `''` | none | Accepted original media location; for an ISO, this stable image path participates in rescan matching when its mount root changes. | `MainFrame::OnAddOrUpdateDiskImage`; `Database::FindDiskBySourcePath`, disk writes | Explicit |
| `disk_type` | `TEXT` | No | none | `CHECK` allowed values | Supported disk classification; mounted ISO is `VirtualDisk`, a Windows removable drive is `RemovableUSB`, and unknown classification is `Other`. | `src/core/Disk.h`; `src/app/MainFrame.cpp`; `src/platform/VolumeInfo.cpp`; disk writes | Explicit |
