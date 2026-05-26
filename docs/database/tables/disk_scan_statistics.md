# `disk_scan_statistics`

## Purpose And Lifecycle

Stores only the latest successful scan statistics for one disk. It is inserted or updated after staged scanning succeeds and is deleted automatically with its disk.

## Keys, Relationships And Constraints

- Primary key and foreign key: `disk_id -> disks.id ON DELETE CASCADE`.
- `calculated_file_crcs` is Boolean-like `INTEGER` constrained to `0` or `1`.
- Archive counts are required non-negative integers for the latest successful scan only.

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description | Used in code | Confidence |
|---|---|---:|---|---|---|---|---|
| `disk_id` | `INTEGER` | No | none | `PRIMARY KEY`, foreign key | Disk whose latest scan is recorded. | `Database::UpdateDiskScanStatistics`; `ScanCoordinator::Start` | Explicit |
| `last_scanned_at` | `INTEGER` | No | none | none | Unix timestamp of latest successful scan completion. | `ScanCoordinator`; statistics upsert | Explicit |
| `image_scanning_time_ms` | `INTEGER` | No | `0` | none | Elapsed scan duration in milliseconds. | `FileScanner::Result`; statistics upsert | Explicit |
| `imported_descriptions_count` | `INTEGER` | No | `0` | none | Count of descriptions imported during scan; currently zero because no importer is implemented. | Statistics upsert | Explicit |
| `calculated_file_crcs` | `INTEGER` | No | `0` | `CHECK (0,1)` | Whether CRC calculation was enabled for the scan. | Add New Disk/Media result; scanner; statistics upsert | Explicit |
| `scanned_archives` | `INTEGER` | No | `0` | `CHECK (>=0)` | Readable physical archives stored as archive-backed folder rows. | `FileScanner::Result`; statistics upsert | Explicit |
| `archive_files_count` | `INTEGER` | No | `0` | `CHECK (>=0)` | Files stored inside successfully expanded archives. | `FileScanner::Result`; statistics upsert | Explicit |
| `archive_folders_count` | `INTEGER` | No | `0` | `CHECK (>=0)` | Explicit or synthesized directory rows below successfully expanded archives, excluding containers. | `FileScanner::Result`; statistics upsert | Explicit |
