# `folders`

## Purpose And Lifecycle

Stores normalized physical and archive-backed folder hierarchy, offline display metadata, and recursive stored file-byte totals for a disk. `FileScanner::ScanFolder()` inserts folders and finalizes `content_size` during staged scanning; deleting disk contents removes folder rows and cascades their files.

## Keys, Relationships And Indexes

- Primary key: `id`.
- `disk_id -> disks.id ON DELETE CASCADE`.
- Nullable `parent_folder_id -> folders.id ON DELETE CASCADE`; null identifies a disk root folder.
- Indexes: `idx_folders_parent(disk_id, parent_folder_id)` and unique `idx_folders_disk_path(disk_id, path COLLATE NOCASE)`.

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description | Used in code | Confidence |
|---|---|---:|---|---|---|---|---|
| `id` | `INTEGER` | No | none | `PRIMARY KEY AUTOINCREMENT` | Folder identifier. | `Database::InsertFolder`; browse/search queries | Explicit |
| `disk_id` | `INTEGER` | No | none | Foreign key; indexed with hierarchy/path | Owning disk. | Scanner; storage hierarchy queries | Explicit |
| `parent_folder_id` | `INTEGER` | Yes | `NULL` | Self foreign key; indexed | Parent folder, absent for stored root. | Scanner; browse/tree queries | Explicit |
| `path` | `TEXT` | No | none | Unique per disk/path index | Stored absolute source path used for navigation identity. | Scanner; browser/tree query scope | Explicit |
| `name` | `TEXT` | No | none | none | Display name of folder. | Scanner; tree/list/search display | Explicit |
| `created_at` | `INTEGER` | No | none | none | Filesystem creation time as Unix timestamp. | Scanner | Explicit |
| `modified_at` | `INTEGER` | No | none | none | Filesystem modification time as Unix timestamp. | Scanner; display projection | Explicit |
| `accessed_at` | `INTEGER` | No | none | none | Filesystem access time as Unix timestamp. | Scanner | Explicit |
| `attributes` | `INTEGER` | No | `0` | none | Win32 attribute bitmask. | Scanner; display projection | Explicit |
| `content_size` | `INTEGER` | No | `0` | none | Sum of stored file sizes in this folder and all stored descendant folders. | `FileScanner`; right-pane/status projection | Explicit |
| `entry_type` | `TEXT` | No | `'directory'` | `CHECK ('directory','archive')` | Distinguishes physical/internal directory rows from readable physical archives represented as folder-like rows. | `FileScanner`; browse/search projection | Explicit |
