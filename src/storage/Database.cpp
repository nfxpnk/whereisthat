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
}

Database::~Database(){ Close(); }
Database::Database(Database&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}
Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        Close();
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}
void Database::Close(){ if(db_) { sqlite3_close(db_); db_ = nullptr; } }
bool Database::CreateNew(const std::wstring& path){
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    if(OpenInternal(path, false)) return true;
    DeleteFileW(path.c_str());
    return false;
}
bool Database::OpenExisting(const std::wstring& path){ return OpenInternal(path, true); }
bool Database::OpenInternal(const std::wstring& path, bool requireExistingSchema){
    Close();
    if(sqlite3_open_v2(wit::platform::ToUtf8(path).c_str(), &db_, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        Close();
        return false;
    }
    if(requireExistingSchema && !HasCatalogSchema()) {
        Close();
        return false;
    }
    if(InitializeSchema()) return true;
    Close();
    return false;
}
bool Database::HasCatalogSchema(){
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
bool Database::Exec(const char* sql){ return sqlite3_exec(db_, sql, nullptr, nullptr, nullptr)==SQLITE_OK; }
bool Database::InitializeSchema(){
    return Exec("PRAGMA foreign_keys=ON;") && Exec("PRAGMA journal_mode=WAL;") && Exec("PRAGMA synchronous=NORMAL;") &&
    Exec("CREATE TABLE IF NOT EXISTS catalogs (id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,root_path TEXT NOT NULL,created_at TEXT NOT NULL,item_count INTEGER NOT NULL DEFAULT 0);") &&
    Exec("CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY AUTOINCREMENT,catalog_id INTEGER NOT NULL,parent_path TEXT NOT NULL,name TEXT NOT NULL,extension TEXT NOT NULL,size INTEGER NOT NULL,modified_at TEXT NOT NULL,attributes INTEGER NOT NULL DEFAULT 0,is_directory INTEGER NOT NULL DEFAULT 0,FOREIGN KEY (catalog_id) REFERENCES catalogs(id) ON DELETE CASCADE);") &&
    (TableHasColumn(db_, "files", "is_directory") || Exec("ALTER TABLE files ADD COLUMN is_directory INTEGER NOT NULL DEFAULT 0;")) &&
    Exec("CREATE INDEX IF NOT EXISTS idx_catalogs_root_path ON catalogs(root_path COLLATE NOCASE);") &&
    Exec("CREATE INDEX IF NOT EXISTS idx_files_catalog_path ON files(catalog_id,parent_path);") && Exec("CREATE INDEX IF NOT EXISTS idx_files_catalog_name ON files(catalog_id,name);") && Exec("CREATE INDEX IF NOT EXISTS idx_files_catalog_extension ON files(catalog_id,extension);") && Exec("CREATE INDEX IF NOT EXISTS idx_files_catalog_size ON files(catalog_id,size);");
}
bool Database::BeginTransaction(){return Exec("BEGIN TRANSACTION;");}
bool Database::Commit(){return Exec("COMMIT;");}
bool Database::Rollback(){return Exec("ROLLBACK;");}
std::int64_t Database::AddCatalog(const wit::core::Catalog& c){ SQLiteStatement s(db_,"INSERT INTO catalogs(name,root_path,created_at,item_count) VALUES(?,?,?,0);"); s.BindText(1,wit::platform::ToUtf8(c.name)); s.BindText(2,wit::platform::ToUtf8(c.rootPath)); s.BindText(3,wit::platform::ToUtf8(c.createdAt)); return sqlite3_step(s.Raw())==SQLITE_DONE?sqlite3_last_insert_rowid(db_):0;} 
std::int64_t Database::FindCatalogByRootPath(const std::wstring& rootPath){ SQLiteStatement s(db_,"SELECT id FROM catalogs WHERE root_path=? COLLATE NOCASE LIMIT 1;"); s.BindText(1,wit::platform::ToUtf8(rootPath)); return sqlite3_step(s.Raw())==SQLITE_ROW?sqlite3_column_int64(s.Raw(),0):0; }
bool Database::DeleteDuplicateCatalogsForRootPath(const std::wstring& rootPath, std::int64_t id){ SQLiteStatement s(db_,"DELETE FROM catalogs WHERE root_path=? COLLATE NOCASE AND id<>?;"); s.BindText(1,wit::platform::ToUtf8(rootPath)); s.BindInt64(2,id); return sqlite3_step(s.Raw())==SQLITE_DONE; }
bool Database::DeleteFilesForCatalog(std::int64_t id){ SQLiteStatement s(db_,"DELETE FROM files WHERE catalog_id=?;"); s.BindInt64(1,id); return sqlite3_step(s.Raw())==SQLITE_DONE; }
bool Database::UpdateCatalogItemCount(std::int64_t id,std::int64_t n){ SQLiteStatement s(db_,"UPDATE catalogs SET item_count=? WHERE id=?;"); s.BindInt64(1,n); s.BindInt64(2,id); return sqlite3_step(s.Raw())==SQLITE_DONE; }
bool Database::InsertFile(const wit::core::FileEntry& f){ SQLiteStatement s(db_,"INSERT INTO files(catalog_id,parent_path,name,extension,size,modified_at,attributes,is_directory) VALUES(?,?,?,?,?,?,?,?);"); s.BindInt64(1,f.catalogId); s.BindText(2,wit::platform::ToUtf8(f.parentPath)); s.BindText(3,wit::platform::ToUtf8(f.name)); s.BindText(4,wit::platform::ToUtf8(f.extension)); s.BindInt64(5,(long long)f.size); s.BindText(6,wit::platform::ToUtf8(f.modifiedAt)); s.BindInt64(7,f.attributes); s.BindInt64(8,f.isDirectory?1:0); return sqlite3_step(s.Raw())==SQLITE_DONE; }
std::vector<wit::core::Catalog> Database::GetCatalogs(){ std::vector<wit::core::Catalog> v; SQLiteStatement s(db_,"SELECT id,name,root_path,created_at,item_count FROM catalogs ORDER BY id DESC;"); while(sqlite3_step(s.Raw())==SQLITE_ROW){ wit::core::Catalog c; c.id=sqlite3_column_int64(s.Raw(),0); c.name=wit::platform::ToUtf16((const char*)sqlite3_column_text(s.Raw(),1)); c.rootPath=wit::platform::ToUtf16((const char*)sqlite3_column_text(s.Raw(),2)); c.createdAt=wit::platform::ToUtf16((const char*)sqlite3_column_text(s.Raw(),3)); c.itemCount=sqlite3_column_int64(s.Raw(),4); v.push_back(c);} return v; }
int Database::GetFileCountForCatalog(std::int64_t id){ SQLiteStatement s(db_,"SELECT COUNT(*) FROM files WHERE catalog_id=?;"); s.BindInt64(1,id); return sqlite3_step(s.Raw())==SQLITE_ROW?sqlite3_column_int(s.Raw(),0):0;}
std::vector<wit::core::FileEntry> Database::GetFilesPageForCatalog(std::int64_t id,int offset,int limit){ std::vector<wit::core::FileEntry> v; SQLiteStatement s(db_,"SELECT id,parent_path,name,extension,size,modified_at,attributes,is_directory FROM files WHERE catalog_id=? ORDER BY is_directory DESC,parent_path,name LIMIT ? OFFSET ?;"); s.BindInt64(1,id); s.BindInt64(2,limit); s.BindInt64(3,offset); while(sqlite3_step(s.Raw())==SQLITE_ROW){ wit::core::FileEntry f; f.id=sqlite3_column_int64(s.Raw(),0); f.catalogId=id; f.parentPath=wit::platform::ToUtf16((const char*)sqlite3_column_text(s.Raw(),1)); f.name=wit::platform::ToUtf16((const char*)sqlite3_column_text(s.Raw(),2)); f.extension=wit::platform::ToUtf16((const char*)sqlite3_column_text(s.Raw(),3)); f.size=sqlite3_column_int64(s.Raw(),4); f.modifiedAt=wit::platform::ToUtf16((const char*)sqlite3_column_text(s.Raw(),5)); f.attributes=static_cast<std::uint32_t>(sqlite3_column_int(s.Raw(),6)); f.isDirectory=sqlite3_column_int(s.Raw(),7)!=0; v.push_back(f);} return v; }
}
