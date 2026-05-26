CREATE TABLE IF NOT EXISTS folders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    disk_id INTEGER NOT NULL,
    parent_folder_id INTEGER,
    path TEXT NOT NULL,
    name TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    modified_at INTEGER NOT NULL,
    accessed_at INTEGER NOT NULL,
    attributes INTEGER NOT NULL DEFAULT 0,
    content_size INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE,
    FOREIGN KEY (parent_folder_id) REFERENCES folders(id) ON DELETE CASCADE
);
