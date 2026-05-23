#include "Database.h"
#include "SQLiteStatement.h"
#include "../platform/Win32Helpers.h"
#include "../../third_party/sqlite/sqlite3.h"
#include <Windows.h>
#include <cstdint>
#include <string>
#include <utility>

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
}

Database::~Database() {
    Close();
}

Database::Database(Database&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        Close();
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

void Database::Close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::CreateNew(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    if (OpenInternal(path, false)) return true;
    DeleteFileW(path.c_str());
    return false;
}

bool Database::OpenExisting(const std::wstring& path) {
    return OpenInternal(path, true);
}

bool Database::OpenInternal(const std::wstring& path, bool requireExistingSchema) {
    Close();
    if (sqlite3_open_v2(wit::platform::ToUtf8(path).c_str(), &db_, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        Close();
        return false;
    }
    if (requireExistingSchema && !HasCatalogSchema()) {
        Close();
        return false;
    }
    if (InitializeSchema()) return true;
    Close();
    return false;
}

bool Database::HasCatalogSchema() {
    return TableHasColumn(db_, "catalogs", "id") &&
        TableHasColumn(db_, "catalogs", "name") &&
        TableHasColumn(db_, "catalogs", "root_path") &&
        TableHasColumn(db_, "catalogs", "created_at") &&
        TableHasColumn(db_, "catalogs", "item_count") &&
        TableHasColumn(db_, "files", "id") &&
        TableHasColumn(db_, "files", "catalog_id") &&
        TableHasColumn(db_, "files", "parent_path") &&
        TableHasColumn(db_, "files", "name") &&
        TableHasColumn(db_, "files", "extension") &&
        TableHasColumn(db_, "files", "size") &&
        TableHasColumn(db_, "files", "modified_at") &&
        TableHasColumn(db_, "files", "attributes");
}

bool Database::Exec(const char* sql) {
    return sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Database::InitializeSchema() {
    return Exec("PRAGMA foreign_keys=ON;") && Exec("PRAGMA journal_mode=WAL;") && Exec("PRAGMA synchronous=NORMAL;") &&
        Exec("CREATE TABLE IF NOT EXISTS catalogs (id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,root_path TEXT NOT NULL,created_at TEXT NOT NULL,item_count INTEGER NOT NULL DEFAULT 0);") &&
        Exec("CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY AUTOINCREMENT,catalog_id INTEGER NOT NULL,parent_path TEXT NOT NULL,name TEXT NOT NULL,extension TEXT NOT NULL,size INTEGER NOT NULL,modified_at TEXT NOT NULL,attributes INTEGER NOT NULL DEFAULT 0,is_directory INTEGER NOT NULL DEFAULT 0,FOREIGN KEY (catalog_id) REFERENCES catalogs(id) ON DELETE CASCADE);") &&
        (TableHasColumn(db_, "files", "is_directory") || Exec("ALTER TABLE files ADD COLUMN is_directory INTEGER NOT NULL DEFAULT 0;")) &&
        Exec("CREATE INDEX IF NOT EXISTS idx_catalogs_root_path ON catalogs(root_path COLLATE NOCASE);") &&
        Exec("CREATE INDEX IF NOT EXISTS idx_files_catalog_path ON files(catalog_id,parent_path);") &&
        Exec("CREATE INDEX IF NOT EXISTS idx_files_catalog_name ON files(catalog_id,name);") &&
        Exec("CREATE INDEX IF NOT EXISTS idx_files_catalog_extension ON files(catalog_id,extension);") &&
        Exec("CREATE INDEX IF NOT EXISTS idx_files_catalog_size ON files(catalog_id,size);");
}

bool Database::BeginTransaction() {
    return Exec("BEGIN TRANSACTION;");
}

bool Database::Commit() {
    return Exec("COMMIT;");
}

bool Database::Rollback() {
    return Exec("ROLLBACK;");
}

std::int64_t Database::AddCatalog(const wit::core::Catalog& catalog) {
    SQLiteStatement statement(db_, "INSERT INTO catalogs(name,root_path,created_at,item_count) VALUES(?,?,?,0);");
    statement.BindText(1, wit::platform::ToUtf8(catalog.name));
    statement.BindText(2, wit::platform::ToUtf8(catalog.rootPath));
    statement.BindText(3, wit::platform::ToUtf8(catalog.createdAt));
    return sqlite3_step(statement.Raw()) == SQLITE_DONE ? sqlite3_last_insert_rowid(db_) : 0;
}

std::int64_t Database::FindCatalogByRootPath(const std::wstring& rootPath) {
    SQLiteStatement statement(db_, "SELECT id FROM catalogs WHERE root_path=? COLLATE NOCASE LIMIT 1;");
    statement.BindText(1, wit::platform::ToUtf8(rootPath));
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int64(statement.Raw(), 0) : 0;
}

bool Database::DeleteDuplicateCatalogsForRootPath(const std::wstring& rootPath, std::int64_t id) {
    SQLiteStatement statement(db_, "DELETE FROM catalogs WHERE root_path=? COLLATE NOCASE AND id<>?;");
    statement.BindText(1, wit::platform::ToUtf8(rootPath));
    statement.BindInt64(2, id);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

bool Database::DeleteFilesForCatalog(std::int64_t id) {
    SQLiteStatement statement(db_, "DELETE FROM files WHERE catalog_id=?;");
    statement.BindInt64(1, id);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

bool Database::UpdateCatalogItemCount(std::int64_t id, std::int64_t count) {
    SQLiteStatement statement(db_, "UPDATE catalogs SET item_count=? WHERE id=?;");
    statement.BindInt64(1, count);
    statement.BindInt64(2, id);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

bool Database::InsertFile(const wit::core::FileEntry& file) {
    SQLiteStatement statement(db_,
        "INSERT INTO files(catalog_id,parent_path,name,extension,size,modified_at,attributes,is_directory) VALUES(?,?,?,?,?,?,?,?);");
    statement.BindInt64(1, file.catalogId);
    statement.BindText(2, wit::platform::ToUtf8(file.parentPath));
    statement.BindText(3, wit::platform::ToUtf8(file.name));
    statement.BindText(4, wit::platform::ToUtf8(file.extension));
    statement.BindInt64(5, static_cast<long long>(file.size));
    statement.BindText(6, wit::platform::ToUtf8(file.modifiedAt));
    statement.BindInt64(7, file.attributes);
    statement.BindInt64(8, file.isDirectory ? 1 : 0);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

std::vector<wit::core::Catalog> Database::GetCatalogs() {
    std::vector<wit::core::Catalog> catalogs;
    SQLiteStatement statement(db_, "SELECT id,name,root_path,created_at,item_count FROM catalogs ORDER BY id DESC;");
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::Catalog catalog;
        catalog.id = sqlite3_column_int64(statement.Raw(), 0);
        catalog.name = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 1)));
        catalog.rootPath = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 2)));
        catalog.createdAt = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 3)));
        catalog.itemCount = sqlite3_column_int64(statement.Raw(), 4);
        catalogs.push_back(catalog);
    }
    return catalogs;
}

int Database::GetFileCountForCatalog(std::int64_t id) {
    SQLiteStatement statement(db_, "SELECT COUNT(*) FROM files WHERE catalog_id=?;");
    statement.BindInt64(1, id);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::FileEntry> Database::GetFilesPageForCatalog(std::int64_t id, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    SQLiteStatement statement(db_,
        "SELECT id,parent_path,name,extension,size,modified_at,attributes,is_directory FROM files WHERE catalog_id=? ORDER BY is_directory DESC,parent_path,name LIMIT ? OFFSET ?;");
    statement.BindInt64(1, id);
    statement.BindInt64(2, limit);
    statement.BindInt64(3, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry file;
        file.id = sqlite3_column_int64(statement.Raw(), 0);
        file.catalogId = id;
        file.parentPath = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 1)));
        file.name = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 2)));
        file.extension = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 3)));
        file.size = sqlite3_column_int64(statement.Raw(), 4);
        file.modifiedAt = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 5)));
        file.attributes = static_cast<std::uint32_t>(sqlite3_column_int(statement.Raw(), 6));
        file.isDirectory = sqlite3_column_int(statement.Raw(), 7) != 0;
        files.push_back(file);
    }
    return files;
}

int Database::GetItemSearchCount(const std::wstring& nameTerm) {
    SQLiteStatement statement(db_, "SELECT COUNT(*) FROM files WHERE name LIKE ? ESCAPE '\\';");
    statement.BindText(1, ItemNameLikePattern(nameTerm));
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::FileEntry> Database::GetItemSearchPage(const std::wstring& nameTerm, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    SQLiteStatement statement(db_,
        "SELECT id,catalog_id,parent_path,name,extension,size,modified_at,attributes,is_directory "
        "FROM files WHERE name LIKE ? ESCAPE '\\' ORDER BY is_directory DESC,name,parent_path LIMIT ? OFFSET ?;");
    statement.BindText(1, ItemNameLikePattern(nameTerm));
    statement.BindInt64(2, limit);
    statement.BindInt64(3, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry file;
        file.id = sqlite3_column_int64(statement.Raw(), 0);
        file.catalogId = sqlite3_column_int64(statement.Raw(), 1);
        file.parentPath = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 2)));
        file.name = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 3)));
        file.extension = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 4)));
        file.size = sqlite3_column_int64(statement.Raw(), 5);
        file.modifiedAt = wit::platform::ToUtf16(reinterpret_cast<const char*>(sqlite3_column_text(statement.Raw(), 6)));
        file.attributes = static_cast<std::uint32_t>(sqlite3_column_int(statement.Raw(), 7));
        file.isDirectory = sqlite3_column_int(statement.Raw(), 8) != 0;
        files.push_back(file);
    }
    return files;
}
}
