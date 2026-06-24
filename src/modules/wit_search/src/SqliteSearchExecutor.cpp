#include "wit_search/SqliteSearchExecutor.h"

#include "wit_database/SQLiteStatement.h"
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"

#include <Windows.h>
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
    const bool hasWildcard = utf8.find('*') != std::string::npos;
    std::string pattern;
    if (!hasWildcard) pattern.push_back('%');

    for (const auto character : utf8) {
        if (character == '*') {
            if (pattern.empty() || pattern.back() != '%') pattern.push_back('%');
            continue;
        }
        if (character == '%' || character == '_' || character == '\\') pattern.push_back('\\');
        pattern.push_back(character);
    }
    if (!hasWildcard) pattern.push_back('%');
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

int NaturalNoCaseCollation(void*, int leftBytes, const void* leftValue, int rightBytes, const void* rightValue) {
    const auto* left = static_cast<const wchar_t*>(leftValue);
    const auto* right = static_cast<const wchar_t*>(rightValue);
    const int result = CompareStringEx(LOCALE_NAME_USER_DEFAULT,
        LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS,
        left, leftBytes / static_cast<int>(sizeof(wchar_t)),
        right, rightBytes / static_cast<int>(sizeof(wchar_t)),
        nullptr, nullptr, 0);
    if (result == CSTR_LESS_THAN) return -1;
    if (result == CSTR_GREATER_THAN) return 1;
    return 0;
}

void EnsureNaturalNoCaseCollation(sqlite3* db) {
    if (!db) return;
    sqlite3_create_collation_v2(db, "WIN_NATURAL_NOCASE", SQLITE_UTF16LE, nullptr,
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

bool ExecSql(sqlite3* db, const char* sql) {
    return db && sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::string SortCacheKey(wit::core::FileSort sort) {
    return std::to_string(static_cast<int>(sort.column)) + (sort.ascending ? ":a" : ":d");
}

std::string NameCacheKey(const std::wstring& nameTerm, wit::core::FileSort sort) {
    const auto term = wit::platform::ToUtf8(nameTerm);
    return "name:" + std::to_string(term.size()) + ":" + term + ":" + SortCacheKey(sort);
}

std::string AdvancedCacheKey(const AdvancedSearchExpression& expression, wit::core::FileSort sort) {
    std::string key{"advanced:"};
    for (std::size_t index = 0; index < expression.criteria.size(); ++index) {
        const auto& criterion = expression.criteria[index];
        key += std::to_string(static_cast<int>(criterion.field)) + ":";
        key += std::to_string(static_cast<int>(criterion.comparison)) + ":";
        const auto text = wit::platform::ToUtf8(criterion.textValue);
        key += std::to_string(text.size()) + ":" + text + ":";
        key += std::to_string(criterion.sizeValue) + ";";
        if (index < expression.logicalOperators.size()) {
            key += std::to_string(static_cast<int>(expression.logicalOperators[index])) + ";";
        }
    }
    return key + SortCacheKey(sort);
}

bool ResetPageCache(sqlite3* db) {
    return ExecSql(db,
        "CREATE TEMP TABLE IF NOT EXISTS wit_search_page_cache("
        "position INTEGER PRIMARY KEY,is_directory INTEGER NOT NULL,item_id INTEGER NOT NULL);") &&
        ExecSql(db, "DELETE FROM wit_search_page_cache;");
}

bool FinishPageCache(sqlite3* db, bool success) {
    if (success) return ExecSql(db, "RELEASE wit_search_cache;");
    ExecSql(db, "ROLLBACK TO wit_search_cache;");
    ExecSql(db, "RELEASE wit_search_cache;");
    return false;
}

bool BuildNamePageCache(sqlite3* db, const std::wstring& nameTerm, wit::core::FileSort sort) {
    if (!ExecSql(db, "SAVEPOINT wit_search_cache;") || !ResetPageCache(db)) {
        return FinishPageCache(db, false);
    }

    const auto pattern = ItemNameLikePattern(nameTerm);
    const auto folderSql = std::string(
        "INSERT INTO wit_search_page_cache(is_directory,item_id) "
        "SELECT 1,c.id FROM folders c LEFT JOIN folders p ON c.parent_folder_id=p.id "
        "WHERE c.name LIKE ? ESCAPE '\\' ") + OrderByFor(sort, true) + ";";
    wit::storage::SQLiteStatement folders(db, folderSql.c_str());
    folders.BindText(1, pattern);
    bool success = sqlite3_step(folders.Raw()) == SQLITE_DONE;

    if (success) {
        const auto fileSql = std::string(
            "INSERT INTO wit_search_page_cache(is_directory,item_id) "
            "SELECT 0,f.id FROM files f JOIN folders p ON f.folder_id=p.id "
            "WHERE f.name LIKE ? ESCAPE '\\' ") + OrderByFor(sort, false) + ";";
        wit::storage::SQLiteStatement files(db, fileSql.c_str());
        files.BindText(1, pattern);
        success = sqlite3_step(files.Raw()) == SQLITE_DONE;
    }
    return FinishPageCache(db, success);
}

bool BuildAdvancedPageCache(
    sqlite3* db, const AdvancedSearchExpression& expression, wit::core::FileSort sort) {
    if (!ExecSql(db, "SAVEPOINT wit_search_cache;") || !ResetPageCache(db)) {
        return FinishPageCache(db, false);
    }

    auto folderWhere = BuildAdvancedWhere(expression, true);
    const auto folderSql = std::string(
        "INSERT INTO wit_search_page_cache(is_directory,item_id) "
        "SELECT 1,c.id FROM folders c LEFT JOIN folders p ON c.parent_folder_id=p.id WHERE ") +
        folderWhere.whereClause + " " + OrderByFor(sort, true) + ";";
    wit::storage::SQLiteStatement folders(db, folderSql.c_str());
    BindAdvancedParams(folders, folderWhere.params);
    bool success = sqlite3_step(folders.Raw()) == SQLITE_DONE;

    if (success) {
        auto fileWhere = BuildAdvancedWhere(expression, false);
        const auto fileSql = std::string(
            "INSERT INTO wit_search_page_cache(is_directory,item_id) "
            "SELECT 0,f.id FROM files f JOIN folders p ON f.folder_id=p.id WHERE ") +
            fileWhere.whereClause + " " + OrderByFor(sort, false) + ";";
        wit::storage::SQLiteStatement files(db, fileSql.c_str());
        BindAdvancedParams(files, fileWhere.params);
        success = sqlite3_step(files.Raw()) == SQLITE_DONE;
    }
    return FinishPageCache(db, success);
}

std::vector<wit::core::FileEntry> ReadPageCache(sqlite3* db, int offset, int limit) {
    std::vector<wit::core::FileEntry> entries;
    if (!db || offset < 0 || limit <= 0) return entries;

    constexpr const char* sql =
        "SELECT id,disk_id,parent_path,name,extension,size,modified_at,attributes,is_directory,entry_type FROM ("
        "SELECT r.position,c.id,c.disk_id,COALESCE(p.path,'') AS parent_path,c.name,'' AS extension,"
        "c.content_size AS size,c.modified_at,c.attributes,1 AS is_directory,c.entry_type "
        "FROM wit_search_page_cache r JOIN folders c ON r.is_directory=1 AND c.id=r.item_id "
        "LEFT JOIN folders p ON c.parent_folder_id=p.id WHERE r.position>? AND r.position<=? "
        "UNION ALL "
        "SELECT r.position,f.id,f.disk_id,p.path AS parent_path,f.name,f.extension,f.size,"
        "f.modified_at,f.attributes,0 AS is_directory,'file' AS entry_type "
        "FROM wit_search_page_cache r JOIN files f ON r.is_directory=0 AND f.id=r.item_id "
        "JOIN folders p ON f.folder_id=p.id WHERE r.position>? AND r.position<=?"
        ") ORDER BY position;";
    wit::storage::SQLiteStatement statement(db, sql);
    const auto end = static_cast<long long>(offset) + limit;
    statement.BindInt64(1, offset);
    statement.BindInt64(2, end);
    statement.BindInt64(3, offset);
    statement.BindInt64(4, end);
    entries.reserve(static_cast<std::size_t>(limit));
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry entry;
        PopulateDisplayEntry(entry, statement.Raw());
        entries.push_back(std::move(entry));
    }
    return entries;
}
}

SqliteSearchExecutor::SqliteSearchExecutor(sqlite3* db) : db_(db) {}

void SqliteSearchExecutor::SetDatabase(sqlite3* db) {
    pageCacheKey_.clear();
    pageCacheValid_ = false;
    db_ = db;
}

int SqliteSearchExecutor::CountByName(const std::wstring& nameTerm) {
    pageCacheValid_ = false;
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
    if (!db_ || limit <= 0) return {};
    EnsureNaturalNoCaseCollation(db_);

    const auto key = NameCacheKey(nameTerm, sort);
    if (!pageCacheValid_ || pageCacheKey_ != key) {
        pageCacheValid_ = BuildNamePageCache(db_, nameTerm, sort);
        pageCacheKey_ = pageCacheValid_ ? key : std::string{};
    }
    return pageCacheValid_ ? ReadPageCache(db_, offset, limit) : std::vector<wit::core::FileEntry>{};
}

int SqliteSearchExecutor::CountAdvanced(const AdvancedSearchExpression& expression) {
    pageCacheValid_ = false;
    if (!db_ || expression.criteria.empty()) return 0;
    return CountAdvancedInTable(db_, expression, true) + CountAdvancedInTable(db_, expression, false);
}

std::vector<wit::core::FileEntry> SqliteSearchExecutor::PageAdvanced(
    const AdvancedSearchExpression& expression, int offset, int limit, wit::core::FileSort sort) {
    if (!db_ || expression.criteria.empty() || limit <= 0) return {};
    EnsureNaturalNoCaseCollation(db_);

    const auto key = AdvancedCacheKey(expression, sort);
    if (!pageCacheValid_ || pageCacheKey_ != key) {
        pageCacheValid_ = BuildAdvancedPageCache(db_, expression, sort);
        pageCacheKey_ = pageCacheValid_ ? key : std::string{};
    }
    return pageCacheValid_ ? ReadPageCache(db_, offset, limit) : std::vector<wit::core::FileEntry>{};
}
}
