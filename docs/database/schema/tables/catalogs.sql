-- Source: src/storage/Database.cpp, Database::InitializeSchema().
-- Despite its historical name, each row is an indexed media source in one catalog database file.
CREATE TABLE IF NOT EXISTS catalogs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    root_path TEXT NOT NULL,
    created_at TEXT NOT NULL,
    item_count INTEGER NOT NULL DEFAULT 0
);
