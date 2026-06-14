#include "wit_search/SqliteSearchExecutor.h"

#include "wit_database/SQLiteStatement.h"
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"

#include <Windows.h>
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

int NaturalNoCaseCollation(void*, int leftBytes, const void* leftValue, int rightBytes, const void* rightValue) {
    const std::string leftUtf8(static_cast<const char*>(leftValue), static_cast<std::size_t>(leftBytes));
    const std::string rightUtf8(static_cast<const char*>(rightValue), static_cast<std::size_t>(rightBytes));
    const auto left = wit::platform::ToUtf16(leftUtf8);
    const auto right = wit::platform::ToUtf16(rightUtf8);
    const int result = CompareStringEx(LOCALE_NAME_USER_DEFAULT,
        LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS,
        left.c_str(), static_cast<int>(left.size()),
        right.c_str(), static_cast<int>(right.size()),
        nullptr, nullptr, 0);
    if (result == CSTR_LESS_THAN) return -1;
    if (result == CSTR_GREATER_THAN) return 1;
    return 0;
}

void EnsureNaturalNoCaseCollation(sqlite3* db) {
    if (!db) return;
    sqlite3_create_collation_v2(db, "WIN_NATURAL_NOCASE", SQLITE_UTF8, nullptr,
        NaturalNoCaseCollation, nullptr);
}

const char* OrderExpressionFor(wit::core::FileSortColumn column, bool folders) {
    switch (column) {
    case wit::core::FileSortColumn::Type:
        return folders ? "c.entry_type COLLATE WIN_NATURAL_NOCASE"
            : "f.extension COLLATE WIN_NATURAL_NOCASE";
    case wit::core::FileSortColumn::Size:
        return folders ? "c.content_size" : "f.size";
    case wit::core::FileSortColumn::Path:
        return "p.path COLLATE WIN_NATURAL_NOCASE";
    case wit::core::FileSortColumn::Modified:
        return folders ? "c.modified_at" : "f.modified_at";
    case wit::core::FileSortColumn::Name:
    default:
        return folders ? "c.name COLLATE WIN_NATURAL_NOCASE" : "f.name COLLATE WIN_NATURAL_NOCASE";
    }
}

std::string OrderByFor(wit::core::FileSort sort, bool folders) {
    std::string order{"ORDER BY "};
    order += OrderExpressionFor(sort.column, folders);
    order += sort.ascending ? " ASC," : " DESC,";
    order += folders ? " c.name COLLATE WIN_NATURAL_NOCASE ASC,c.id ASC "
        : " f.name COLLATE WIN_NATURAL_NOCASE ASC,f.id ASC ";
    return order;
}
}

SqliteSearchExecutor::SqliteSearchExecutor(sqlite3* db) : db_(db) {}

void SqliteSearchExecutor::SetDatabase(sqlite3* db) {
    cachedFolderCountTerm_.clear();
    cachedFolderCount_ = 0;
    hasCachedFolderCount_ = false;
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
    const std::wstring& nameTerm, int offset, int limit, wit::core::FileSort sort) {
    std::vector<wit::core::FileEntry> files;
    const auto pattern = ItemNameLikePattern(nameTerm);
    EnsureNaturalNoCaseCollation(db_);

    if (!hasCachedFolderCount_ || cachedFolderCountTerm_ != nameTerm) {
        wit::storage::SQLiteStatement folderCountStatement(db_,
            "SELECT COUNT(*) FROM folders WHERE name LIKE ? ESCAPE '\\';");
        folderCountStatement.BindText(1, pattern);
        cachedFolderCount_ = sqlite3_step(folderCountStatement.Raw()) == SQLITE_ROW
            ? sqlite3_column_int(folderCountStatement.Raw(), 0) : 0;
        cachedFolderCountTerm_ = nameTerm;
        hasCachedFolderCount_ = true;
    }
    const int folderCount = cachedFolderCount_;

    if (offset < folderCount && limit > 0) {
        const auto sql = std::string(
            "SELECT c.id,c.disk_id,COALESCE(p.path,''),c.name,'',c.content_size,c.modified_at,c.attributes,1,c.entry_type "
            "FROM folders c LEFT JOIN folders p ON c.parent_folder_id=p.id "
            "WHERE c.name LIKE ? ESCAPE '\\' ") + OrderByFor(sort, true) + "LIMIT ? OFFSET ?;";
        wit::storage::SQLiteStatement folderStatement(db_, sql.c_str());
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
        const auto sql = std::string(
            "SELECT f.id,f.disk_id,p.path,f.name,f.extension,f.size,f.modified_at,f.attributes,0,'file' "
            "FROM files f JOIN folders p ON f.folder_id=p.id "
            "WHERE f.name LIKE ? ESCAPE '\\' ") + OrderByFor(sort, false) + "LIMIT ? OFFSET ?;";
        wit::storage::SQLiteStatement fileStatement(db_, sql.c_str());
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
