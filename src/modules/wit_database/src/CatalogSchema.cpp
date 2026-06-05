#include "wit_database/CatalogSchema.h"
#include "wit_database/SqliteConnection.h"
#include "resource.h"
#include "third_party/sqlite/sqlite3.h"
#include <array>
#include <string>
#include <windows.h>

namespace wit::storage {
namespace {
constexpr std::array<int, 6> kSchemaTableResources = {
    IDR_SQL_TABLE_CATALOG_METADATA,
    IDR_SQL_TABLE_DISK_GROUPS,
    IDR_SQL_TABLE_DISKS,
    IDR_SQL_TABLE_DISK_SCAN_STATISTICS,
    IDR_SQL_TABLE_FOLDERS,
    IDR_SQL_TABLE_FILES,
};

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

std::string LoadSqlResource(int resourceId) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) return {};
    HGLOBAL handle = LoadResource(module, resource);
    if (!handle) return {};
    const auto* data = static_cast<const char*>(LockResource(handle));
    const DWORD size = SizeofResource(module, resource);
    if (!data || size == 0) return {};
    return std::string(data, data + size);
}

bool ExecSqlResource(SqliteConnection& connection, int resourceId) {
    const std::string sql = LoadSqlResource(resourceId);
    return !sql.empty() && connection.Exec(sql.c_str());
}

bool EnsureDiskGroupParentColumn(SqliteConnection& connection) {
    if (TableHasColumn(connection.Raw(), "disk_groups", "parent_group_id")) return true;
    return connection.Exec("ALTER TABLE disk_groups ADD COLUMN parent_group_id INTEGER;");
}
}

bool CatalogSchema::Initialize(SqliteConnection& connection) {
    if (!ExecSqlResource(connection, IDR_SQL_PRAGMAS)) return false;
    for (const int tableResource : kSchemaTableResources) {
        if (!ExecSqlResource(connection, tableResource)) return false;
    }
    return connection.Exec("INSERT OR IGNORE INTO catalog_metadata(id,description) VALUES(1,'');") &&
        EnsureDiskGroupParentColumn(connection) &&
        EnsureIndexes(connection);
}

bool CatalogSchema::EnsureIndexes(SqliteConnection& connection) {
    return ExecSqlResource(connection, IDR_SQL_INDEXES);
}

bool CatalogSchema::Validate(SqliteConnection& connection) {
    return TableHasColumn(connection.Raw(), "catalog_metadata", "description") &&
        TableHasColumn(connection.Raw(), "disk_groups", "parent_group_id") &&
        TableHasColumn(connection.Raw(), "disk_groups", "name") &&
        TableHasColumn(connection.Raw(), "disk_groups", "updated_at") &&
        TableHasColumn(connection.Raw(), "disks", "disk_group_id") &&
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
