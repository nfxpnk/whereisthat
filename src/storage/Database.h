#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../core/BrowserLocation.h"
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
    bool CreateWorkingCopy(const Database& source);
    bool ReplaceCatalogDataFrom(const Database& source);
    void Close();
    bool IsOpen() const { return db_ != nullptr; }
    bool IsEditable() const { return editable_; }
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
    int GetBrowserItemCount(const wit::core::BrowserLocation& location);
    std::vector<wit::core::FileEntry> GetBrowserItemsPage(const wit::core::BrowserLocation& location,
        int offset, int limit);
    std::vector<wit::core::FileEntry> GetChildFolders(std::int64_t sourceId, const std::wstring& parentPath);
    int GetItemSearchCount(const std::wstring& nameTerm);
    std::vector<wit::core::FileEntry> GetItemSearchPage(const std::wstring& nameTerm, int offset, int limit);
private:
    bool OpenInternal(const std::wstring& path, bool requireExistingSchema, bool readOnly = false);
    bool HasCatalogSchema();
    bool Exec(const char* sql);
    sqlite3* db_{};
    bool editable_{};
};
}
