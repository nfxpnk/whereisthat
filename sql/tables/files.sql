CREATE TABLE IF NOT EXISTS files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    disk_id INTEGER NOT NULL,
    folder_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    extension TEXT NOT NULL DEFAULT '',
    crc TEXT,
    size INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    modified_at INTEGER NOT NULL,
    accessed_at INTEGER NOT NULL,
    attributes INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE,
    FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE CASCADE
);
