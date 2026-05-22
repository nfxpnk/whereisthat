#pragma once
#include <string>
#include <vector>
#include "../core/Catalog.h"
#include "../core/FileEntry.h"
struct sqlite3;

namespace wit::storage {
class Database {
public:
    ~Database();
    bool Open(const std::wstring& path);
    bool InitializeSchema();
    bool BeginTransaction();
    bool Commit();
    bool Rollback();
    std::int64_t AddCatalog(const wit::core::Catalog& catalog);
    bool UpdateCatalogItemCount(std::int64_t catalogId, std::int64_t itemCount);
    bool InsertFile(const wit::core::FileEntry& file);
    std::vector<wit::core::Catalog> GetCatalogs();
    int GetFileCountForCatalog(std::int64_t catalogId);
    std::vector<wit::core::FileEntry> GetFilesPageForCatalog(std::int64_t catalogId, int offset, int limit);
private:
    bool Exec(const char* sql);
    sqlite3* db_{};
};
}
