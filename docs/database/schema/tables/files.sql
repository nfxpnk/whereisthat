-- Source: src/storage/Database.cpp, Database::InitializeSchema().
CREATE TABLE IF NOT EXISTS files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    catalog_id INTEGER NOT NULL,
    parent_path TEXT NOT NULL,
    name TEXT NOT NULL,
    extension TEXT NOT NULL,
    size INTEGER NOT NULL,
    modified_at TEXT NOT NULL,
    attributes INTEGER NOT NULL DEFAULT 0,
    is_directory INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (catalog_id) REFERENCES catalogs(id) ON DELETE CASCADE
);
