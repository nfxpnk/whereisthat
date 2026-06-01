#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "wit_database/SqliteConnection.h"
#include "wit_types/BrowserLocation.h"
#include "wit_catalog/Catalog.h"
#include <wit_types/Disk.h>
#include <wit_types/FileEntry.h>
#include "wit_types/FolderEntry.h"

namespace wit::storage {
class Database {
public:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    bool CreateNew(const std::wstring& path, bool overwriteExisting);
    bool OpenExisting(const std::wstring& path);
    bool CreateWorkingCopy(const Database& source);
    bool ReplaceCatalogDataFrom(const Database& source);
    void Close();
    bool IsOpen() const { return connection_.IsOpen(); }
    bool IsEditable() const { return editable_; }
    bool InitializeSchema();
    bool BeginTransaction();
    bool Commit();
    bool Rollback();
    bool SetCatalogDescription(const std::wstring& description);
    wit::core::CatalogMetadata GetCatalogMetadata();
    wit::core::CatalogSummary GetCatalogSummary() const;
    std::int64_t CreateDiskGroup(const std::wstring& name);
    std::vector<wit::core::DiskGroup> GetDiskGroups();
    std::int64_t AddDisk(const wit::core::Disk& disk);
    std::int64_t FindDiskBySourcePath(const std::wstring& sourcePath,
        const std::wstring& originalLocation = L"");
    bool DeleteContentForDisk(std::int64_t diskId);
    bool UpdateDisk(const wit::core::Disk& disk);
    bool UpdateDiskScanStatistics(const wit::core::DiskScanStatistics& statistics);
    int GetDiskCount();
    std::vector<wit::core::Disk> GetDisksPage(int offset, int limit);
    int GetBrowserRootItemCount(const wit::core::BrowserLocation& location);
    std::vector<wit::core::BrowserItem> GetBrowserRootItemsPage(
        const wit::core::BrowserLocation& location, int offset, int limit);
    std::int64_t InsertFolder(const wit::core::FolderEntry& folder);
    bool UpdateFolderContentSize(std::int64_t folderId, std::uint64_t contentSize);
    bool InsertFile(const wit::core::FileEntry& file);
    std::vector<wit::core::Catalog> GetCatalogs();
    int GetBrowserItemCount(const wit::core::BrowserLocation& location);
    std::vector<wit::core::FileEntry> GetBrowserItemsPage(const wit::core::BrowserLocation& location,
        int offset, int limit);
    bool HasChildFolders(std::int64_t sourceId, const std::wstring& parentPath);
    std::vector<wit::core::FileEntry> GetChildFolders(std::int64_t sourceId, const std::wstring& parentPath);
    int GetItemSearchCount(const std::wstring& nameTerm);
    std::vector<wit::core::FileEntry> GetItemSearchPage(const std::wstring& nameTerm, int offset, int limit);
private:
    bool OpenInternal(const std::wstring& path, bool requireExistingSchema, bool readOnly = false);
    bool HasCatalogSchema();
    bool Exec(const char* sql);
    SqliteConnection connection_;
    bool editable_{};
};
}

