#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../core/Catalog.h"
#include "../core/FileEntry.h"
struct sqlite3;

namespace wit::storage {
class Database {
public:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    bool CreateNew(const std::wstring& path);
    bool OpenExisting(const std::wstring& path);
    void Close();
    bool IsOpen() const { return db_ != nullptr; }
    bool InitializeSchema();
    bool BeginTransaction();
    bool Commit();
    bool Rollback();
    std::int64_t AddCatalog(const wit::core::Catalog& catalog);
    std::int64_t FindCatalogByRootPath(const std::wstring& rootPath);
    bool DeleteDuplicateCatalogsForRootPath(const std::wstring& rootPath, std::int64_t catalogIdToKeep);
    bool DeleteFilesForCatalog(std::int64_t catalogId);
    bool UpdateCatalogItemCount(std::int64_t catalogId, std::int64_t itemCount);
    bool InsertFile(const wit::core::FileEntry& file);
    std::vector<wit::core::Catalog> GetCatalogs();
    int GetFileCountForCatalog(std::int64_t catalogId);
    std::vector<wit::core::FileEntry> GetFilesPageForCatalog(std::int64_t catalogId, int offset, int limit);
private:
    bool OpenInternal(const std::wstring& path, bool requireExistingSchema);
    bool HasCatalogSchema();
    bool Exec(const char* sql);
    sqlite3* db_{};
};
}
