-- Source: src/storage/Database.cpp, Database::InitializeSchema().
-- This statement is applied only when PRAGMA table_info(files) reports no is_directory column.
ALTER TABLE files ADD COLUMN is_directory INTEGER NOT NULL DEFAULT 0;
