-- Source: src/storage/Database.cpp, Database::OpenInternal() and Database::InitializeSchema().
-- foreign_keys is enabled on writable initialization and read-only open.
PRAGMA foreign_keys = ON;

-- These settings are issued only by writable schema initialization.
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
