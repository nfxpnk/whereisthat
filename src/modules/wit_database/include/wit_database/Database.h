#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "wit_database/SqliteBrowserRepository.h"
#include "wit_database/SqliteConnection.h"
#include "wit_database/SQLiteStatement.h"
#include "wit_types/BrowserLocation.h"
#include "wit_catalog/Catalog.h"
#include <wit_search/SqliteSearchExecutor.h>
#include <wit_types/Disk.h>
#include <wit_types/FileEntry.h>
#include "wit_types/FolderEntry.h"

namespace wit::infra {
struct ScanProfile;
}

namespace wit::storage {
namespace infra = ::wit::infra;

class Database {
public:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    [[nodiscard]] bool CreateNew(const std::wstring& path, bool overwriteExisting);
    [[nodiscard]] bool OpenExisting(const std::wstring& path);
    [[nodiscard]] bool CreateWorkingCopy(const Database& source);
    [[nodiscard]] bool SaveCatalogDataFrom(const Database& source);
    void Close();
    bool IsOpen() const { return connection_.IsOpen(); }
    bool IsEditable() const { return editable_; }
    [[nodiscard]] bool InitializeSchema();
    [[nodiscard]] bool BeginTransaction();
    [[nodiscard]] bool Commit();
    [[nodiscard]] bool Rollback();
    [[nodiscard]] bool SetCatalogDescription(const std::wstring& description);
    wit::core::CatalogMetadata GetCatalogMetadata();
    wit::core::CatalogSummary GetCatalogSummary() const;
    [[nodiscard]] std::int64_t CreateDiskGroup(const std::wstring& name);
    std::vector<wit::core::DiskGroup> GetDiskGroups();
    [[nodiscard]] std::int64_t AddDisk(const wit::core::Disk& disk);
    [[nodiscard]] std::int64_t FindDiskBySourcePath(const std::wstring& sourcePath,
        const std::wstring& originalLocation = L"");
    [[nodiscard]] bool DeleteContentForDisk(std::int64_t diskId);
    [[nodiscard]] bool MoveDiskToGroup(std::int64_t diskId, std::int64_t diskGroupId);
    [[nodiscard]] bool MoveDiskGroupToGroup(std::int64_t diskGroupId, std::int64_t parentGroupId);
    [[nodiscard]] bool UpdateDisk(const wit::core::Disk& disk);
    [[nodiscard]] bool UpdateDiskScanStatistics(const wit::core::DiskScanStatistics& statistics);
    int GetDiskCount();
    std::vector<wit::core::Disk> GetDisksPage(int offset, int limit);
    int GetBrowserRootItemCount(const wit::core::BrowserLocation& location);
    std::vector<wit::core::BrowserItem> GetBrowserRootItemsPage(
        const wit::core::BrowserLocation& location, int offset, int limit);
    [[nodiscard]] std::int64_t InsertFolder(const wit::core::FolderEntry& folder,
        infra::ScanProfile* profile = nullptr);
    [[nodiscard]] bool UpdateFolderContentSize(std::int64_t folderId, std::uint64_t contentSize,
        infra::ScanProfile* profile = nullptr);
    [[nodiscard]] bool InsertFile(const wit::core::FileEntry& file, infra::ScanProfile* profile = nullptr);
    std::vector<wit::core::Catalog> GetCatalogs();
    int GetBrowserItemCount(const wit::core::BrowserLocation& location);
    std::vector<wit::core::FileEntry> GetBrowserItemsPage(const wit::core::BrowserLocation& location,
        int offset, int limit);
    bool HasChildFolders(std::int64_t sourceId, const std::wstring& parentPath);
    std::vector<wit::core::FileEntry> GetChildFolders(std::int64_t sourceId, const std::wstring& parentPath);
    int GetItemSearchCount(const std::wstring& nameTerm);
    std::vector<wit::core::FileEntry> GetItemSearchPage(const std::wstring& nameTerm, int offset, int limit);
    IBrowserRepository& BrowserRepository() { return browserRepository_; }
    wit::search::ISearchRepository& SearchRepository() { return searchRepository_; }
private:
    [[nodiscard]] bool OpenInternal(const std::wstring& path, bool requireExistingSchema, bool readOnly = false);
    [[nodiscard]] bool HasCatalogSchema();
    [[nodiscard]] bool Exec(const char* sql);
    void RebindRepositories();
    [[nodiscard]] bool PrepareScanStatements();
    void FinalizeScanStatements();
    [[nodiscard]] SQLiteStatement* ScanInsertFolderStatement(infra::ScanProfile* profile);
    [[nodiscard]] SQLiteStatement* ScanUpdateFolderContentSizeStatement(infra::ScanProfile* profile);
    [[nodiscard]] SQLiteStatement* ScanInsertFileStatement(infra::ScanProfile* profile);
    SqliteConnection connection_;
    SqliteBrowserRepository browserRepository_{nullptr};
    wit::search::SqliteSearchExecutor searchRepository_{nullptr};
    std::unique_ptr<SQLiteStatement> insertFolderStatement_;
    std::unique_ptr<SQLiteStatement> updateFolderContentSizeStatement_;
    std::unique_ptr<SQLiteStatement> insertFileStatement_;
    bool editable_{};
};
}

