# `disk_groups`

## Purpose And Lifecycle

Stores optional virtual root folders used to group disks/media inside a catalog. A disk with `disks.disk_group_id` set appears under that group; a disk with `NULL` remains directly under the catalog root.

## Keys, Relationships, Indexes And Constraints

- Primary key: `id`.
- `name` is unique case-insensitively.
- `disks.disk_group_id` references this table and is set to `NULL` if a group is deleted.
- Index: `idx_disk_groups_name` on `name COLLATE NOCASE`.

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description |
|---|---|---:|---|---|---|
| `id` | `INTEGER` | No | none | `PRIMARY KEY AUTOINCREMENT` | Disk group identifier. |
| `name` | `TEXT` | No | none | `UNIQUE`; `idx_disk_groups_name` | User-visible virtual folder name. |
| `created_at` | `INTEGER` | No | none | none | Unix timestamp when the group was created. |
| `updated_at` | `INTEGER` | No | none | none | Unix timestamp when the group was last changed. |
