CREATE TABLE IF NOT EXISTS catalog_metadata (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    description TEXT NOT NULL DEFAULT ''
);
