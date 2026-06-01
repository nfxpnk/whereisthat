#include "wit_database/SqliteBrowserRepository.h"

#include "wit_database/SQLiteStatement.h"
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"

#include <cstdint>
#include <string>

namespace wit::storage {
namespace {
std::wstring Text(sqlite3_stmt* stmt, int column) {
    const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
    return value ? wit::platform::ToUtf16(value) : std::wstring{};
}

void PopulateDisplayEntry(wit::core::FileEntry& entry, sqlite3_stmt* stmt) {
    entry.id = sqlite3_column_int64(stmt, 0);
    entry.catalogId = sqlite3_column_int64(stmt, 1);
    entry.parentPath = Text(stmt, 2);
    entry.name = Text(stmt, 3);
    entry.extension = Text(stmt, 4);
    entry.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
    entry.modifiedAtValue = sqlite3_column_int64(stmt, 6);
    entry.modifiedAt = wit::platform::FormatUnixTimestamp(entry.modifiedAtValue);
    entry.attributes = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
    entry.isDirectory = sqlite3_column_int(stmt, 8) != 0;
    entry.isArchive = Text(stmt, 9) == L"archive";
}
}

SqliteBrowserRepository::SqliteBrowserRepository(sqlite3* db) : db_(db) {}

int SqliteBrowserRepository::GetBrowserItemCount(const wit::core::BrowserLocation& location) {
    if (location.isRoot) {
        SQLiteStatement statement(db_, "SELECT COUNT(*) FROM disks;");
        return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
    }
    SQLiteStatement statement(db_,
        "WITH parent(id) AS (SELECT id FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT (SELECT COUNT(*) FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=?) + "
        "(SELECT COUNT(*) FROM files f JOIN parent p ON f.folder_id=p.id);");
    statement.BindInt64(1, location.sourceId);
    statement.BindText(2, wit::platform::ToUtf8(location.path));
    statement.BindInt64(3, location.sourceId);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::FileEntry> SqliteBrowserRepository::GetBrowserItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    if (location.isRoot) {
        SQLiteStatement statement(db_,
            "SELECT id,id,source_path,disk_name,'',0,updated_at,0,1,'directory' FROM disks ORDER BY disk_name LIMIT ? OFFSET ?;");
        statement.BindInt64(1, limit);
        statement.BindInt64(2, offset);
        while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
            wit::core::FileEntry file;
            PopulateDisplayEntry(file, statement.Raw());
            files.push_back(file);
        }
        return files;
    }
    SQLiteStatement statement(db_,
        "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT id,disk_id,parent_path,name,extension,size,modified_at,attributes,is_directory,entry_type FROM ("
        "SELECT c.id,c.disk_id,p.path AS parent_path,c.name,'' AS extension,c.content_size AS size,c.modified_at,c.attributes,1 AS is_directory,c.entry_type "
        "FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=? "
        "UNION ALL SELECT f.id,f.disk_id,p.path AS parent_path,f.name,f.extension,f.size,f.modified_at,f.attributes,0 AS is_directory,'file' AS entry_type "
        "FROM files f JOIN parent p ON f.folder_id=p.id)"
        " ORDER BY is_directory DESC,name LIMIT ? OFFSET ?;");
    statement.BindInt64(1, location.sourceId);
    statement.BindText(2, wit::platform::ToUtf8(location.path));
    statement.BindInt64(3, location.sourceId);
    statement.BindInt64(4, limit);
    statement.BindInt64(5, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry file;
        PopulateDisplayEntry(file, statement.Raw());
        files.push_back(file);
    }
    return files;
}

bool SqliteBrowserRepository::HasChildFolders(std::int64_t sourceId, const std::wstring& parentPath) {
    SQLiteStatement statement(db_,
        "WITH parent(id) AS (SELECT id FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT EXISTS(SELECT 1 FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=?);");
    statement.BindInt64(1, sourceId);
    statement.BindText(2, wit::platform::ToUtf8(parentPath));
    statement.BindInt64(3, sourceId);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW && sqlite3_column_int(statement.Raw(), 0) != 0;
}

std::vector<wit::core::FileEntry> SqliteBrowserRepository::GetChildFolders(
    std::int64_t sourceId, const std::wstring& parentPath) {
    std::vector<wit::core::FileEntry> folders;
    SQLiteStatement statement(db_,
        "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT c.id,c.disk_id,p.path,c.name,'',c.content_size,c.modified_at,c.attributes,1,c.entry_type "
        "FROM folders c JOIN parent p ON c.parent_folder_id=p.id "
        "WHERE c.disk_id=? ORDER BY c.name;");
    statement.BindInt64(1, sourceId);
    statement.BindText(2, wit::platform::ToUtf8(parentPath));
    statement.BindInt64(3, sourceId);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry folder;
        PopulateDisplayEntry(folder, statement.Raw());
        folders.push_back(folder);
    }
    return folders;
}
}
