# `files`

## Purpose And Lifecycle

Stores file-only scanned metadata under normalized folders. `FileScanner::ScanFolder()` inserts physical non-directory items and, when archive browsing is enabled, files inside successfully expanded archive-backed folders; a readable archive container itself is represented in `folders` instead. Rows are removed transitively when refreshed disk folders are deleted.

## Keys, Relationships And Indexes

- Primary key: `id`.
- `disk_id -> disks.id ON DELETE CASCADE`; `folder_id -> folders.id ON DELETE CASCADE`.
- Indexes: `idx_files_folder(folder_id)`, `idx_files_disk_name(disk_id, name)`, and `idx_files_extension(extension)`.
- `crc` is nullable when CRC calculation is disabled or cannot produce a value.

## Attribute Semantics

`attributes` stores the Win32 bitmask returned during enumeration, including `FILE_ATTRIBUTE_HIDDEN`, `FILE_ATTRIBUTE_SYSTEM`, `FILE_ATTRIBUTE_READONLY`, `FILE_ATTRIBUTE_COMPRESSED`, and `FILE_ATTRIBUTE_ARCHIVE` when those flags are present.

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description | Used in code | Confidence |
|---|---|---:|---|---|---|---|---|
| `id` | `INTEGER` | No | none | `PRIMARY KEY AUTOINCREMENT` | File identifier. | Storage insert/browse/search | Explicit |
| `disk_id` | `INTEGER` | No | none | Foreign key; `idx_files_disk_name` | Owning disk. | Scanner; storage projections | Explicit |
| `folder_id` | `INTEGER` | No | none | Foreign key; `idx_files_folder` | Containing stored folder. | Scanner; browser/search joins | Explicit |
| `name` | `TEXT` | No | none | `idx_files_disk_name` | Filename. | Scanner; list/search display | Explicit |
| `description` | `TEXT` | No | `''` | none | Free-text file description. | `Database::InsertFile`; currently no importer/editor | Explicit |
| `extension` | `TEXT` | No | `''` | `idx_files_extension` | Extension without a leading dot; empty for no extension. | `PathHelpers::FileExtension`; display/storage | Explicit |
| `crc` | `TEXT` | Yes | `NULL` | none | Uppercase hexadecimal CRC-32 when calculated. | `FileScanner::FileCrc32`; storage insert | Explicit |
| `size` | `INTEGER` | No | none | none | File size in bytes. | Scanner; list/status display | Explicit |
| `created_at` | `INTEGER` | No | none | none | Creation time as Unix timestamp. | Scanner | Explicit |
| `modified_at` | `INTEGER` | No | none | none | Modification time as Unix timestamp. | Scanner; display projection | Explicit |
| `accessed_at` | `INTEGER` | No | none | none | Last access time as Unix timestamp. | Scanner | Explicit |
| `attributes` | `INTEGER` | No | `0` | none | Win32 filesystem attribute bitmask. | Scanner; display projection | Explicit |
