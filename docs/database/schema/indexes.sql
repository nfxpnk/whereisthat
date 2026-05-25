-- Source: src/storage/Database.cpp, Database::InitializeSchema().
CREATE INDEX IF NOT EXISTS idx_catalogs_root_path
    ON catalogs(root_path COLLATE NOCASE);

CREATE INDEX IF NOT EXISTS idx_files_catalog_path
    ON files(catalog_id, parent_path);

CREATE INDEX IF NOT EXISTS idx_files_catalog_name
    ON files(catalog_id, name);

CREATE INDEX IF NOT EXISTS idx_files_catalog_extension
    ON files(catalog_id, extension);

CREATE INDEX IF NOT EXISTS idx_files_catalog_size
    ON files(catalog_id, size);
