#include "wit_search/SqliteSearchExecutor.h"

#include "wit_database/SQLiteStatement.h"
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <variant>
#include <vector>

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

struct SqlParamValue {
    std::variant<long long, std::string> value;
};

struct AdvancedSql {
    std::string whereClause;
    std::vector<SqlParamValue> params;
};

std::string ComparisonSql(AdvancedSearchOperator op) {
    switch (op) {
    case AdvancedSearchOperator::Equal: return "=";
    case AdvancedSearchOperator::Less: return "<";
    case AdvancedSearchOperator::LessOrEqual: return "<=";
    case AdvancedSearchOperator::Greater: return ">";
    case AdvancedSearchOperator::GreaterOrEqual: return ">=";
    default: return "=";
    }
}

std::string LogicalSql(AdvancedSearchLogicalOperator op) {
    switch (op) {
    case AdvancedSearchLogicalOperator::And: return "AND";
    case AdvancedSearchLogicalOperator::Or: return "OR";
    case AdvancedSearchLogicalOperator::Xor: return "<>";
    default: return "AND";
    }
}

std::string AdvancedColumnSql(AdvancedSearchField field, bool folders) {
    switch (field) {
    case AdvancedSearchField::Filename: return folders ? "c.name" : "f.name";
    case AdvancedSearchField::Filesize: return folders ? "c.content_size" : "f.size";
    default: return folders ? "c.name" : "f.name";
    }
}

void AppendCriterionSql(const AdvancedSearchCriterion& criterion, bool folders, AdvancedSql& sql) {
    sql.whereClause += "(" + AdvancedColumnSql(criterion.field, folders) + " " +
        ComparisonSql(criterion.comparison) + " ?)";
    if (criterion.field == AdvancedSearchField::Filename) {
        // Advanced filename search is exact-match. Quick Search remains substring-based.
        sql.params.push_back({wit::platform::ToUtf8(criterion.textValue)});
    } else {
        const auto max = static_cast<std::uint64_t>((std::numeric_limits<long long>::max)());
        sql.params.push_back({static_cast<long long>((std::min)(criterion.sizeValue, max))});
    }
}

AdvancedSql BuildAdvancedWhere(const AdvancedSearchExpression& expression, bool folders) {
    AdvancedSql sql;
    if (expression.criteria.empty()) {
        sql.whereClause = "1=0";
        return sql;
    }

    AppendCriterionSql(expression.criteria.front(), folders, sql);
    for (std::size_t index = 1; index < expression.criteria.size(); ++index) {
        const auto logical = expression.logicalOperators[index - 1];
        const std::string join = LogicalSql(logical);
        AdvancedSql criterionSql;
        AppendCriterionSql(expression.criteria[index], folders, criterionSql);
        sql.whereClause = "((" + sql.whereClause + ") " + join + " (" + criterionSql.whereClause + "))";
        std::ranges::move(criterionSql.params, std::back_inserter(sql.params));
    }
    return sql;
}

void BindAdvancedParams(wit::storage::SQLiteStatement& statement, const std::vector<SqlParamValue>& params, int start = 1) {
    int index = start;
    for (const auto& param : params) {
        if (const auto* value = std::get_if<long long>(&param.value)) {
            statement.BindInt64(index, *value);
        } else if (const auto* value = std::get_if<std::string>(&param.value)) {
            statement.BindText(index, *value);
        }
        ++index;
    }
}

int CountAdvancedInTable(sqlite3* db, const AdvancedSearchExpression& expression, bool folders) {
    auto sql = BuildAdvancedWhere(expression, folders);
    const std::string statementSql = folders
        ? "SELECT COUNT(*) FROM folders c WHERE " + sql.whereClause + ";"
        : "SELECT COUNT(*) FROM files f WHERE " + sql.whereClause + ";";
    wit::storage::SQLiteStatement statement(db, statementSql.c_str());
    BindAdvancedParams(statement, sql.params);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
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
    const std::wstring& nameTerm, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    const auto pattern = ItemNameLikePattern(nameTerm);

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

int SqliteSearchExecutor::CountAdvanced(const AdvancedSearchExpression& expression) {
    if (!db_ || expression.criteria.empty()) return 0;
    return CountAdvancedInTable(db_, expression, true) + CountAdvancedInTable(db_, expression, false);
}

std::vector<wit::core::FileEntry> SqliteSearchExecutor::PageAdvanced(
    const AdvancedSearchExpression& expression, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    if (!db_ || expression.criteria.empty() || limit <= 0) return files;

    const int folderCount = CountAdvancedInTable(db_, expression, true);
    if (offset < folderCount) {
        auto folderSql = BuildAdvancedWhere(expression, true);
        const std::string statementSql =
            "SELECT c.id,c.disk_id,COALESCE(p.path,''),c.name,'',c.content_size,c.modified_at,c.attributes,1,c.entry_type "
            "FROM folders c LEFT JOIN folders p ON c.parent_folder_id=p.id "
            "WHERE " + folderSql.whereClause + " ORDER BY c.name,c.id LIMIT ? OFFSET ?;";
        wit::storage::SQLiteStatement folderStatement(db_, statementSql.c_str());
        BindAdvancedParams(folderStatement, folderSql.params);
        const int paramStart = static_cast<int>(folderSql.params.size()) + 1;
        folderStatement.BindInt64(paramStart, limit);
        folderStatement.BindInt64(paramStart + 1, offset);
        while (sqlite3_step(folderStatement.Raw()) == SQLITE_ROW) {
            wit::core::FileEntry file;
            PopulateDisplayEntry(file, folderStatement.Raw());
            files.push_back(file);
        }
    }

    const int remaining = limit - static_cast<int>(files.size());
    if (remaining > 0) {
        const int fileOffset = (std::max)(0, offset - folderCount);
        auto fileSql = BuildAdvancedWhere(expression, false);
        const std::string statementSql =
            "SELECT f.id,f.disk_id,p.path,f.name,f.extension,f.size,f.modified_at,f.attributes,0,'file' "
            "FROM files f JOIN folders p ON f.folder_id=p.id "
            "WHERE " + fileSql.whereClause + " ORDER BY f.name,f.id LIMIT ? OFFSET ?;";
        wit::storage::SQLiteStatement fileStatement(db_, statementSql.c_str());
        BindAdvancedParams(fileStatement, fileSql.params);
        const int paramStart = static_cast<int>(fileSql.params.size()) + 1;
        fileStatement.BindInt64(paramStart, remaining);
        fileStatement.BindInt64(paramStart + 1, fileOffset);
        while (sqlite3_step(fileStatement.Raw()) == SQLITE_ROW) {
            wit::core::FileEntry file;
            PopulateDisplayEntry(file, fileStatement.Raw());
            files.push_back(file);
        }
    }
    return files;
}
}
