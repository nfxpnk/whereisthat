#include "wit_search/SqliteSearchExecutor.h"

#include "wit_database/SQLiteStatement.h"
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace wit::search {
namespace {
std::string ItemNameLikePattern(const std::wstring& term) {
    const auto utf8 = wit::platform::ToUtf8(term);
    std::string pattern{"%"};
    for (const auto character : utf8) {
        if (character == '%' || character == '_' || character == '\\') pattern.push_back('\\');
        pattern.push_back(character);
    }
    pattern.push_back('%');
    return pattern;
}

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
    entry.modifiedAt = sqlite3_column_int64(stmt, 6);
    entry.attributes = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
    entry.isDirectory = sqlite3_column_int(stmt, 8) != 0;
    entry.isArchive = Text(stmt, 9) == L"archive";
}
}

SqliteSearchExecutor::SqliteSearchExecutor(sqlite3* db) : db_(db) {}

void SqliteSearchExecutor::SetDatabase(sqlite3* db) {
    db_ = db;
}

int SqliteSearchExecutor::CountByName(const std::wstring& nameTerm) {
    wit::storage::SQLiteStatement statement(db_,
        "SELECT (SELECT COUNT(*) FROM files WHERE name LIKE ? ESCAPE '\\') + "
        "(SELECT COUNT(*) FROM folders WHERE name LIKE ? ESCAPE '\\');");
    const auto pattern = ItemNameLikePattern(nameTerm);
    statement.BindText(1, pattern);
    statement.BindText(2, pattern);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::FileEntry> SqliteSearchExecutor::PageByName(
    const std::wstring& nameTerm, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    const auto pattern = ItemNameLikePattern(nameTerm);

    wit::storage::SQLiteStatement folderCountStatement(db_,
        "SELECT COUNT(*) FROM folders WHERE name LIKE ? ESCAPE '\\';");
    folderCountStatement.BindText(1, pattern);
    const int folderCount = sqlite3_step(folderCountStatement.Raw()) == SQLITE_ROW
        ? sqlite3_column_int(folderCountStatement.Raw(), 0) : 0;

    if (offset < folderCount && limit > 0) {
        wit::storage::SQLiteStatement folderStatement(db_,
            "SELECT c.id,c.disk_id,COALESCE(p.path,''),c.name,'',c.content_size,c.modified_at,c.attributes,1,c.entry_type "
            "FROM folders c LEFT JOIN folders p ON c.parent_folder_id=p.id "
            "WHERE c.name LIKE ? ESCAPE '\\' ORDER BY c.name,c.id LIMIT ? OFFSET ?;");
        folderStatement.BindText(1, pattern);
        folderStatement.BindInt64(2, limit);
        folderStatement.BindInt64(3, offset);
        while (sqlite3_step(folderStatement.Raw()) == SQLITE_ROW) {
            wit::core::FileEntry file;
            PopulateDisplayEntry(file, folderStatement.Raw());
            files.push_back(file);
        }
    }

    const int remaining = limit - static_cast<int>(files.size());
    if (remaining > 0) {
        const int fileOffset = (std::max)(0, offset - folderCount);
        wit::storage::SQLiteStatement fileStatement(db_,
            "SELECT f.id,f.disk_id,p.path,f.name,f.extension,f.size,f.modified_at,f.attributes,0,'file' "
            "FROM files f JOIN folders p ON f.folder_id=p.id "
            "WHERE f.name LIKE ? ESCAPE '\\' ORDER BY f.name,f.id LIMIT ? OFFSET ?;");
        fileStatement.BindText(1, pattern);
        fileStatement.BindInt64(2, remaining);
        fileStatement.BindInt64(3, fileOffset);
        while (sqlite3_step(fileStatement.Raw()) == SQLITE_ROW) {
            wit::core::FileEntry file;
            PopulateDisplayEntry(file, fileStatement.Raw());
            files.push_back(file);
        }
    }
    return files;
}
}
