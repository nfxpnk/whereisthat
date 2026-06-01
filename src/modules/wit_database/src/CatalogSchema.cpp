#include "wit_database/CatalogSchema.h"
#include "wit_database/SqliteConnection.h"
#include "third_party/sqlite/sqlite3.h"
#include <string>

namespace wit::storage {
namespace {
bool TableHasColumn(sqlite3* db, const char* table, const char* expectedColumn) {
    sqlite3_stmt* stmt{};
    const std::string query = "PRAGMA table_info(" + std::string(table) + ");";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name && std::string(name) == expectedColumn) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}
}

bool CatalogSchema::Initialize(SqliteConnection& connection) {
    return connection.Exec("PRAGMA foreign_keys=ON;") &&
        connection.Exec("PRAGMA journal_mode=WAL;") &&
        connection.Exec("PRAGMA synchronous=NORMAL;") &&
        connection.Exec("CREATE TABLE IF NOT EXISTS catalog_metadata (id INTEGER PRIMARY KEY CHECK (id=1),description TEXT NOT NULL DEFAULT '');") &&
        connection.Exec("CREATE TABLE IF NOT EXISTS disks (id INTEGER PRIMARY KEY AUTOINCREMENT,disk_name TEXT NOT NULL,disk_number INTEGER NOT NULL DEFAULT 0,source_path TEXT NOT NULL COLLATE NOCASE UNIQUE,volume_label TEXT NOT NULL DEFAULT '',total_capacity INTEGER NOT NULL DEFAULT 0,free_space INTEGER NOT NULL DEFAULT 0,cluster_size INTEGER NOT NULL DEFAULT 0,serial_number TEXT NOT NULL DEFAULT '',file_system TEXT NOT NULL DEFAULT '',total_files INTEGER NOT NULL DEFAULT 0,total_folders INTEGER NOT NULL DEFAULT 0,added_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,description TEXT NOT NULL DEFAULT '',category TEXT NOT NULL DEFAULT '',location TEXT NOT NULL DEFAULT '',disk_type TEXT NOT NULL CHECK (disk_type IN ('CD','DVD','BluRay','HardDisk','SolidStateDisk','RemovableUSB','VirtualDisk','Other')));") &&
        connection.Exec("CREATE TABLE IF NOT EXISTS disk_scan_statistics (disk_id INTEGER PRIMARY KEY,last_scanned_at INTEGER NOT NULL,image_scanning_time_ms INTEGER NOT NULL DEFAULT 0,imported_descriptions_count INTEGER NOT NULL DEFAULT 0,calculated_file_crcs INTEGER NOT NULL DEFAULT 0 CHECK (calculated_file_crcs IN (0,1)),scanned_archives INTEGER NOT NULL DEFAULT 0 CHECK (scanned_archives >= 0),archive_files_count INTEGER NOT NULL DEFAULT 0 CHECK (archive_files_count >= 0),archive_folders_count INTEGER NOT NULL DEFAULT 0 CHECK (archive_folders_count >= 0),FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE);") &&
        connection.Exec("CREATE TABLE IF NOT EXISTS folders (id INTEGER PRIMARY KEY AUTOINCREMENT,disk_id INTEGER NOT NULL,parent_folder_id INTEGER,path TEXT NOT NULL,name TEXT NOT NULL,created_at INTEGER NOT NULL,modified_at INTEGER NOT NULL,accessed_at INTEGER NOT NULL,attributes INTEGER NOT NULL DEFAULT 0,content_size INTEGER NOT NULL DEFAULT 0,entry_type TEXT NOT NULL DEFAULT 'directory' CHECK (entry_type IN ('directory','archive')),FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE,FOREIGN KEY (parent_folder_id) REFERENCES folders(id) ON DELETE CASCADE);") &&
        connection.Exec("CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY AUTOINCREMENT,disk_id INTEGER NOT NULL,folder_id INTEGER NOT NULL,name TEXT NOT NULL,description TEXT NOT NULL DEFAULT '',extension TEXT NOT NULL DEFAULT '',crc TEXT,size INTEGER NOT NULL,created_at INTEGER NOT NULL,modified_at INTEGER NOT NULL,accessed_at INTEGER NOT NULL,attributes INTEGER NOT NULL DEFAULT 0,FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE,FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE CASCADE);") &&
        connection.Exec("INSERT OR IGNORE INTO catalog_metadata(id,description) VALUES(1,'');") &&
        connection.Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_disks_source_path ON disks(source_path COLLATE NOCASE);") &&
        connection.Exec("CREATE INDEX IF NOT EXISTS idx_folders_parent ON folders(disk_id,parent_folder_id);") &&
        connection.Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_folders_disk_path ON folders(disk_id,path COLLATE NOCASE);") &&
        connection.Exec("CREATE INDEX IF NOT EXISTS idx_files_folder ON files(folder_id);") &&
        connection.Exec("CREATE INDEX IF NOT EXISTS idx_files_disk_name ON files(disk_id,name);") &&
        connection.Exec("CREATE INDEX IF NOT EXISTS idx_files_extension ON files(extension);");
}

bool CatalogSchema::Validate(SqliteConnection& connection) {
    return TableHasColumn(connection.Raw(), "catalog_metadata", "description") &&
        TableHasColumn(connection.Raw(), "disks", "disk_name") &&
        TableHasColumn(connection.Raw(), "disks", "source_path") &&
        TableHasColumn(connection.Raw(), "disks", "disk_type") &&
        TableHasColumn(connection.Raw(), "disk_scan_statistics", "last_scanned_at") &&
        TableHasColumn(connection.Raw(), "disk_scan_statistics", "scanned_archives") &&
        TableHasColumn(connection.Raw(), "disk_scan_statistics", "archive_files_count") &&
        TableHasColumn(connection.Raw(), "disk_scan_statistics", "archive_folders_count") &&
        TableHasColumn(connection.Raw(), "folders", "parent_folder_id") &&
        TableHasColumn(connection.Raw(), "folders", "path") &&
        TableHasColumn(connection.Raw(), "folders", "content_size") &&
        TableHasColumn(connection.Raw(), "folders", "entry_type") &&
        TableHasColumn(connection.Raw(), "files", "folder_id") &&
        TableHasColumn(connection.Raw(), "files", "extension") &&
        TableHasColumn(connection.Raw(), "files", "crc") &&
        TableHasColumn(connection.Raw(), "files", "accessed_at");
}
}
