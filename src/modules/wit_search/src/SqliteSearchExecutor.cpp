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

struct TableCountResult {
    int count{};
    int sqliteResult{SQLITE_OK};
};

TableCountResult CountAdvancedInTable(
    sqlite3* db, const AdvancedSearchExpression& expression, bool folders) {
    auto sql = BuildAdvancedWhere(expression, folders);
    const std::string statementSql = folders
        ? "SELECT COUNT(*) FROM folders c WHERE " + sql.whereClause + ";"
        : "SELECT COUNT(*) FROM files f WHERE " + sql.whereClause + ";";
    wit::storage::SQLiteStatement statement(db, statementSql.c_str());
    if (!statement.IsValid()) return {0, sqlite3_errcode(db)};
    BindAdvancedParams(statement, sql.params);
    const int stepResult = sqlite3_step(statement.Raw());
    return stepResult == SQLITE_ROW
        ? TableCountResult{sqlite3_column_int(statement.Raw(), 0), SQLITE_OK}
        : TableCountResult{0, stepResult};
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

const char* CombinedOrderExpressionFor(wit::core::FileSortColumn column) {
    switch (column) {
    case wit::core::FileSortColumn::Type:
        return "sort_type COLLATE WIN_NATURAL_NOCASE";
    case wit::core::FileSortColumn::Size:
        return "size";
    case wit::core::FileSortColumn::Path:
        return "parent_path COLLATE WIN_NATURAL_NOCASE";
    case wit::core::FileSortColumn::Modified:
        return "modified_at";
    case wit::core::FileSortColumn::Name:
    default:
        return "name COLLATE WIN_NATURAL_NOCASE";
    }
}

std::string CombinedOrderByFor(wit::core::FileSort sort) {
    std::string order{"ORDER BY "};
    order += CombinedOrderExpressionFor(sort.column);
    order += sort.ascending ? " ASC," : " DESC,";
    order += " name COLLATE WIN_NATURAL_NOCASE ASC,is_directory DESC,id ASC ";
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
        "position INTEGER PRIMARY KEY,id INTEGER NOT NULL,disk_id INTEGER NOT NULL,"
        "parent_path TEXT NOT NULL,name TEXT NOT NULL,extension TEXT NOT NULL,size INTEGER NOT NULL,"
        "modified_at INTEGER NOT NULL,attributes INTEGER NOT NULL,is_directory INTEGER NOT NULL,"
        "entry_type TEXT NOT NULL);") &&
        ExecSql(db, "DELETE FROM wit_search_page_cache;");
}

bool FinishPageCache(sqlite3* db, bool success) {
    if (success) return ExecSql(db, "RELEASE wit_search_cache;");
    ExecSql(db, "ROLLBACK TO wit_search_cache;");
    ExecSql(db, "RELEASE wit_search_cache;");
    return false;
}

std::string CombinedCacheInsertSql(
    const std::string& folderWhereClause,
    const std::string& fileWhereClause,
    wit::core::FileSort sort) {
    return "INSERT INTO wit_search_page_cache("
        "id,disk_id,parent_path,name,extension,size,modified_at,attributes,is_directory,entry_type) "
        "SELECT id,disk_id,parent_path,name,extension,size,modified_at,attributes,is_directory,entry_type FROM ("
        "SELECT c.id,c.disk_id,COALESCE(p.path,'') AS parent_path,c.name,'' AS extension,"
        "c.content_size AS size,c.modified_at,c.attributes,1 AS is_directory,"
        "COALESCE(c.entry_type,'directory') AS entry_type,"
        "COALESCE(c.entry_type,'directory') AS sort_type "
        "FROM folders c LEFT JOIN folders p ON c.parent_folder_id=p.id WHERE " +
        folderWhereClause +
        " UNION ALL "
        "SELECT f.id,f.disk_id,COALESCE(p.path,'') AS parent_path,f.name,"
        "COALESCE(f.extension,'') AS extension,f.size,f.modified_at,f.attributes,"
        "0 AS is_directory,'file' AS entry_type,COALESCE(f.extension,'') AS sort_type "
        "FROM files f JOIN folders p ON f.folder_id=p.id WHERE " +
        fileWhereClause + ") AS combined " + CombinedOrderByFor(sort) + ";";
}

bool BuildNamePageCache(sqlite3* db, const std::wstring& nameTerm, wit::core::FileSort sort) {
    if (!ExecSql(db, "SAVEPOINT wit_search_cache;") || !ResetPageCache(db)) {
        return FinishPageCache(db, false);
    }

    const auto pattern = ItemNameLikePattern(nameTerm);
    const auto sql = CombinedCacheInsertSql(
        "c.name LIKE ? ESCAPE '\\'", "f.name LIKE ? ESCAPE '\\'", sort);
    wit::storage::SQLiteStatement statement(db, sql.c_str());
    statement.BindText(1, pattern);
    statement.BindText(2, pattern);
    const bool success = sqlite3_step(statement.Raw()) == SQLITE_DONE;
    return FinishPageCache(db, success);
}

bool BuildAdvancedPageCache(
    sqlite3* db, const AdvancedSearchExpression& expression, wit::core::FileSort sort) {
    if (!ExecSql(db, "SAVEPOINT wit_search_cache;") || !ResetPageCache(db)) {
        return FinishPageCache(db, false);
    }

    auto folderWhere = BuildAdvancedWhere(expression, true);
    auto fileWhere = BuildAdvancedWhere(expression, false);
    const auto sql = CombinedCacheInsertSql(
        folderWhere.whereClause, fileWhere.whereClause, sort);
    wit::storage::SQLiteStatement statement(db, sql.c_str());
    BindAdvancedParams(statement, folderWhere.params);
    BindAdvancedParams(statement, fileWhere.params,
        static_cast<int>(folderWhere.params.size()) + 1);
    const bool success = sqlite3_step(statement.Raw()) == SQLITE_DONE;
    return FinishPageCache(db, success);
}
struct PageCacheBuildResult {
    bool success{};
    bool generationChanged{};
    std::string generation;
};

template <typename BuildCache, typename ReadGeneration>
PageCacheBuildResult BuildPageCacheWithStableGeneration(
    bool validateGeneration, BuildCache&& buildCache, ReadGeneration&& readGeneration) {
    PageCacheBuildResult result;
    if (!validateGeneration) {
        result.success = buildCache();
        if (result.success) result.generation = readGeneration();
        return result;
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        const auto generation = readGeneration();
        if (!buildCache()) return result;
        if (readGeneration() == generation) {
            result.success = true;
            result.generation = generation;
            return result;
        }
        result.generationChanged = true;
    }
    return result;
}

struct PageReadResult {
    std::vector<wit::core::FileEntry> entries;
    int sqliteResult{SQLITE_OK};
};

PageReadResult ReadPageCache(sqlite3* db, int offset, int limit) {
    PageReadResult result;
    if (!db || offset < 0 || limit <= 0) return result;

    constexpr const char* sql =
        "SELECT id,disk_id,parent_path,name,extension,size,modified_at,attributes,is_directory,entry_type "
        "FROM wit_search_page_cache WHERE position>? AND position<=? ORDER BY position;";
    wit::storage::SQLiteStatement statement(db, sql);
    if (!statement.IsValid()) {
        result.sqliteResult = sqlite3_errcode(db);
        return result;
    }
    const auto end = static_cast<long long>(offset) + limit;
    statement.BindInt64(1, offset);
    statement.BindInt64(2, end);
    result.entries.reserve(static_cast<std::size_t>(limit));
    int stepResult{};
    while ((stepResult = sqlite3_step(statement.Raw())) == SQLITE_ROW) {
        wit::core::FileEntry entry;
        PopulateDisplayEntry(entry, statement.Raw());
        result.entries.push_back(std::move(entry));
    }
    result.sqliteResult = stepResult == SQLITE_DONE ? SQLITE_OK : stepResult;
    return result;
}
}

SqliteSearchExecutor::SqliteSearchExecutor(sqlite3* db) {
    SetDatabase(db);
}

SqliteSearchExecutor::~SqliteSearchExecutor() {
    CancelPending();
    std::scoped_lock lock(operationMutex_);
    CloseSearchDatabase();
}

void SqliteSearchExecutor::CloseSearchDatabase() {
    if (ownsSearchDb_ && searchDb_) sqlite3_close(searchDb_);
    searchDb_ = nullptr;
    ownsSearchDb_ = false;
}

sqlite3* SqliteSearchExecutor::ActiveDatabase() const {
    return searchDb_ ? searchDb_ : sourceDb_;
}

void SqliteSearchExecutor::SetDatabase(sqlite3* db) {
    CancelPending();
    std::scoped_lock lock(operationMutex_);
    CloseSearchDatabase();
    sourceDb_ = db;
    pageCacheKey_.clear();
    pageCacheQueryKey_.clear();
    pageCacheValid_ = false;
    pageCachePinned_ = false;
    {
        std::scoped_lock errorLock(errorMutex_);
        lastError_.clear();
    }
    if (!db) return;

    const char* filename = sqlite3_db_filename(db, "main");
    if (!filename || filename[0] == '\0') {
        searchDb_ = db;
        return;
    }

    sqlite3* candidate{};
    const int flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(filename, &candidate, flags, nullptr) == SQLITE_OK) {
        searchDb_ = candidate;
        ownsSearchDb_ = true;
        sqlite3_busy_timeout(searchDb_, 3000);
    } else {
        if (candidate) sqlite3_close(candidate);
        searchDb_ = db;
    }
}

std::string SqliteSearchExecutor::DatabaseGenerationKey() const {
    sqlite3* db = ActiveDatabase();
    long long dataVersion{};
    if (db) {
        wit::storage::SQLiteStatement statement(db, "PRAGMA data_version;");
        if (sqlite3_step(statement.Raw()) == SQLITE_ROW) dataVersion = sqlite3_column_int64(statement.Raw(), 0);
    }
    const auto sourceChanges = db && db == sourceDb_ ? sqlite3_total_changes64(sourceDb_) : 0;
    return std::to_string(dataVersion) + ":" + std::to_string(sourceChanges);
}

void SqliteSearchExecutor::SetLastError(sqlite3* db, const wchar_t* fallback) {
    std::wstring message = fallback ? fallback : L"Search failed.";
    if (db) {
        const char* detail = sqlite3_errmsg(db);
        if (detail && detail[0] != '\0') {
            message += L" ";
            message += wit::platform::ToUtf16(detail);
        }
    }
    std::scoped_lock lock(errorMutex_);
    lastError_ = std::move(message);
}

void SqliteSearchExecutor::CancelPending() {
    sqlite3* db = searchDb_ ? searchDb_ : sourceDb_;
    if (db) sqlite3_interrupt(db);
}

std::wstring SqliteSearchExecutor::LastErrorMessage() const {
    std::scoped_lock lock(errorMutex_);
    return lastError_;
}

int SqliteSearchExecutor::PageCacheCountLocked(sqlite3* db) {
    wit::storage::SQLiteStatement statement(db, "SELECT COUNT(*) FROM wit_search_page_cache;");
    if (statement.IsValid() && sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        return sqlite3_column_int(statement.Raw(), 0);
    }
    SetLastError(db, L"Could not count prepared search results.");
    pageCacheValid_ = false;
    pageCachePinned_ = false;
    pageCacheKey_.clear();
    pageCacheQueryKey_.clear();
    return 0;
}

PreparedSearchResult SqliteSearchExecutor::PrepareByName(
    const std::wstring& nameTerm, int limit, wit::core::FileSort sort) {
    std::scoped_lock lock(operationMutex_);
    pageCachePinned_ = false;
    PreparedSearchResult result;
    result.entries = PageByNameLocked(nameTerm, 0, limit, sort);
    if (!LastErrorMessage().empty()) return result;
    result.total = PageCacheCountLocked(ActiveDatabase());
    if (pageCacheValid_) pageCachePinned_ = true;
    return result;
}

PreparedSearchResult SqliteSearchExecutor::PrepareAdvanced(
    const AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort) {
    std::scoped_lock lock(operationMutex_);
    pageCachePinned_ = false;
    PreparedSearchResult result;
    result.entries = PageAdvancedLocked(expression, 0, limit, sort);
    if (!LastErrorMessage().empty()) return result;
    result.total = PageCacheCountLocked(ActiveDatabase());
    if (pageCacheValid_) pageCachePinned_ = true;
    return result;
}

int SqliteSearchExecutor::CountByName(const std::wstring& nameTerm) {
    std::scoped_lock lock(operationMutex_);
    sqlite3* db = ActiveDatabase();
    pageCacheValid_ = false;
    pageCachePinned_ = false;
    pageCacheQueryKey_.clear();
    {
        std::scoped_lock errorLock(errorMutex_);
        lastError_.clear();
    }
    if (!db) return 0;
    wit::storage::SQLiteStatement statement(db,
        "SELECT (SELECT COUNT(*) FROM files WHERE name LIKE ? ESCAPE '\\') + "
        "(SELECT COUNT(*) FROM folders WHERE name LIKE ? ESCAPE '\\');");
    const auto pattern = ItemNameLikePattern(nameTerm);
    statement.BindText(1, pattern);
    statement.BindText(2, pattern);
    if (sqlite3_step(statement.Raw()) == SQLITE_ROW) return sqlite3_column_int(statement.Raw(), 0);
    SetLastError(db, L"Could not count search results.");
    return 0;
}

std::vector<wit::core::FileEntry> SqliteSearchExecutor::PageByName(
    const std::wstring& nameTerm, int offset, int limit, wit::core::FileSort sort) {
    std::scoped_lock lock(operationMutex_);
    return PageByNameLocked(nameTerm, offset, limit, sort);
}

std::vector<wit::core::FileEntry> SqliteSearchExecutor::PageByNameLocked(
    const std::wstring& nameTerm, int offset, int limit, wit::core::FileSort sort) {
    sqlite3* db = ActiveDatabase();
    if (!db || limit <= 0) return {};
    EnsureNaturalNoCaseCollation(db);

    {
        std::scoped_lock errorLock(errorMutex_);
        lastError_.clear();
    }
    const auto queryKey = NameCacheKey(nameTerm, sort);
    const auto currentKey = DatabaseGenerationKey() + ":" + queryKey;
    const bool reusePrepared = pageCachePinned_ && pageCacheValid_ && pageCacheQueryKey_ == queryKey;
    if (!reusePrepared && (!pageCacheValid_ || pageCacheKey_ != currentKey)) {
        pageCacheValid_ = false;
        pageCachePinned_ = false;
        pageCacheKey_.clear();
        pageCacheQueryKey_.clear();
        const auto build = BuildPageCacheWithStableGeneration(ownsSearchDb_,
            [&] { return BuildNamePageCache(db, nameTerm, sort); },
            [&] { return DatabaseGenerationKey(); });
        pageCacheValid_ = build.success;
        if (build.success) {
            pageCacheKey_ = build.generation + ":" + queryKey;
            pageCacheQueryKey_ = queryKey;
        }
        if (!pageCacheValid_) {
            SetLastError(db, sqlite3_errcode(db) == SQLITE_INTERRUPT
                ? L"Search was cancelled."
                : build.generationChanged ? L"The catalog changed while preparing search results. Please retry."
                : L"Could not prepare search results.");
            return {};
        }
    }
    auto page = ReadPageCache(db, offset, limit);
    if (page.sqliteResult != SQLITE_OK) {
        pageCacheValid_ = false;
        pageCachePinned_ = false;
        pageCacheKey_.clear();
        pageCacheQueryKey_.clear();
        SetLastError(db, page.sqliteResult == SQLITE_INTERRUPT
            ? L"Search was cancelled." : L"Could not read search results.");
        return {};
    }
    return std::move(page.entries);
}

int SqliteSearchExecutor::CountAdvanced(const AdvancedSearchExpression& expression) {
    std::scoped_lock lock(operationMutex_);
    sqlite3* db = ActiveDatabase();
    pageCacheValid_ = false;
    pageCachePinned_ = false;
    pageCacheQueryKey_.clear();
    {
        std::scoped_lock errorLock(errorMutex_);
        lastError_.clear();
    }
    if (!db || expression.criteria.empty()) return 0;
    const auto folders = CountAdvancedInTable(db, expression, true);
    if (folders.sqliteResult != SQLITE_OK) {
        SetLastError(db, L"Could not count search results.");
        return 0;
    }
    const auto files = CountAdvancedInTable(db, expression, false);
    if (files.sqliteResult != SQLITE_OK) {
        SetLastError(db, L"Could not count search results.");
        return 0;
    }
    return folders.count + files.count;
}
std::vector<wit::core::FileEntry> SqliteSearchExecutor::PageAdvanced(
    const AdvancedSearchExpression& expression, int offset, int limit, wit::core::FileSort sort) {
    std::scoped_lock lock(operationMutex_);
    return PageAdvancedLocked(expression, offset, limit, sort);
}

std::vector<wit::core::FileEntry> SqliteSearchExecutor::PageAdvancedLocked(
    const AdvancedSearchExpression& expression, int offset, int limit, wit::core::FileSort sort) {
    sqlite3* db = ActiveDatabase();
    if (!db || expression.criteria.empty() || limit <= 0) return {};
    EnsureNaturalNoCaseCollation(db);

    {
        std::scoped_lock errorLock(errorMutex_);
        lastError_.clear();
    }
    const auto queryKey = AdvancedCacheKey(expression, sort);
    const auto currentKey = DatabaseGenerationKey() + ":" + queryKey;
    const bool reusePrepared = pageCachePinned_ && pageCacheValid_ && pageCacheQueryKey_ == queryKey;
    if (!reusePrepared && (!pageCacheValid_ || pageCacheKey_ != currentKey)) {
        pageCacheValid_ = false;
        pageCachePinned_ = false;
        pageCacheKey_.clear();
        pageCacheQueryKey_.clear();
        const auto build = BuildPageCacheWithStableGeneration(ownsSearchDb_,
            [&] { return BuildAdvancedPageCache(db, expression, sort); },
            [&] { return DatabaseGenerationKey(); });
        pageCacheValid_ = build.success;
        if (build.success) {
            pageCacheKey_ = build.generation + ":" + queryKey;
            pageCacheQueryKey_ = queryKey;
        }
        if (!pageCacheValid_) {
            SetLastError(db, sqlite3_errcode(db) == SQLITE_INTERRUPT
                ? L"Search was cancelled."
                : build.generationChanged ? L"The catalog changed while preparing search results. Please retry."
                : L"Could not prepare search results.");
            return {};
        }
    }
    auto page = ReadPageCache(db, offset, limit);
    if (page.sqliteResult != SQLITE_OK) {
        pageCacheValid_ = false;
        pageCachePinned_ = false;
        pageCacheKey_.clear();
        pageCacheQueryKey_.clear();
        SetLastError(db, page.sqliteResult == SQLITE_INTERRUPT
            ? L"Search was cancelled." : L"Could not read search results.");
        return {};
    }
    return std::move(page.entries);
}
}
