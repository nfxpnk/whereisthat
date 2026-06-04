CREATE TABLE IF NOT EXISTS disk_groups (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    parent_group_id INTEGER,
    name TEXT NOT NULL COLLATE NOCASE UNIQUE,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    FOREIGN KEY(parent_group_id) REFERENCES disk_groups(id) ON DELETE SET NULL
);
