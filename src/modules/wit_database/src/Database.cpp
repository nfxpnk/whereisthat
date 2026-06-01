#include "wit_database/CatalogSchema.h"
#include "wit_database/Database.h"
#include "wit_database/SQLiteStatement.h"
#include "wit_database/SqliteBrowserRepository.h"
#include <wit_infra/VolumeInfo.h>
#include <wit_infra/Win32Helpers.h>
#include <wit_search/SqliteSearchExecutor.h>
#include "third_party/sqlite/sqlite3.h"
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace wit::storage {
namespace {
std::wstring Text(sqlite3_stmt* stmt, int column) {
    const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
    return value ? wit::platform::ToUtf16(value) : std::wstring{};
}

wit::core::DiskType DiskTypeFromText(const std::wstring& value) {
    if (value == L"CD") return wit::core::DiskType::CD;
    if (value == L"DVD") return wit::core::DiskType::DVD;
    if (value == L"BluRay") return wit::core::DiskType::BluRay;
    if (value == L"HardDisk") return wit::core::DiskType::HardDisk;
    if (value == L"SolidStateDisk") return wit::core::DiskType::SolidStateDisk;
    if (value == L"RemovableUSB") return wit::core::DiskType::RemovableUSB;
    if (value == L"VirtualDisk") return wit::core::DiskType::VirtualDisk;
    return wit::core::DiskType::Other;
}

void PopulateDisk(wit::core::Disk& disk, sqlite3_stmt* stmt) {
    disk.id = sqlite3_column_int64(stmt, 0);
    disk.diskGroupId = sqlite3_column_type(stmt, 1) == SQLITE_NULL ? 0 : sqlite3_column_int64(stmt, 1);
    disk.diskName = Text(stmt, 2);
    disk.diskNumber = sqlite3_column_int64(stmt, 3);
    disk.sourcePath = Text(stmt, 4);
    disk.totalCapacity = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
    disk.freeSpace = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
    disk.updatedAt = sqlite3_column_int64(stmt, 7);
    disk.description = Text(stmt, 8);
    disk.category = Text(stmt, 9);
    disk.location = Text(stmt, 10);
    disk.diskType = DiskTypeFromText(Text(stmt, 11));
}
}

Database::~Database() {
    Close();
}

Database::Database(Database&& other) noexcept
    : connection_(std::move(other.connection_)), editable_(std::exchange(other.editable_, false)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        Close();
        connection_ = std::move(other.connection_);
        editable_ = std::exchange(other.editable_, false);
    }
    return *this;
}

void Database::Close() {
    connection_.Close();
    editable_ = false;
}

bool Database::CreateNew(const std::wstring& path, bool overwriteExisting) {
    const DWORD disposition = overwriteExisting ? CREATE_ALWAYS : CREATE_NEW;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    if (OpenInternal(path, false)) return true;
    DeleteFileW(path.c_str());
    return false;
}

bool Database::OpenExisting(const std::wstring& path) {
    return OpenInternal(path, true) || OpenInternal(path, true, true);
}

bool Database::OpenInternal(const std::wstring& path, bool requireExistingSchema, bool readOnly) {
    Close();
    const int flags = readOnly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
    if (!connection_.Open(path, flags)) return false;
    editable_ = !readOnly;
    if (requireExistingSchema && !HasCatalogSchema()) {
        Close();
        return false;
    }
    if (readOnly) return Exec("PRAGMA foreign_keys=ON;");
    if (!requireExistingSchema && InitializeSchema()) return true;
    if (requireExistingSchema && Exec("PRAGMA foreign_keys=ON;")) return true;
    Close();
    return false;
}

bool Database::CreateWorkingCopy(const Database& source) {
    if (!source.connection_.IsOpen()) return false;
    Close();
    if (!connection_.OpenMemory()) return false;
    editable_ = true;
    auto* backup = sqlite3_backup_init(connection_.Raw(), "main", source.connection_.Raw(), "main");
    if (!backup) {
        Close();
        return false;
    }
    const int stepResult = sqlite3_backup_step(backup, -1);
    const int finishResult = sqlite3_backup_finish(backup);
    const bool success = stepResult == SQLITE_DONE && finishResult == SQLITE_OK && Exec("PRAGMA foreign_keys=ON;");
    if (!success) Close();
    return success;
}

bool Database::ReplaceCatalogDataFrom(const Database& source) {
    if (!connection_.IsOpen() || !editable_ || !source.connection_.IsOpen()) return false;
    auto* backup = sqlite3_backup_init(connection_.Raw(), "main", source.connection_.Raw(), "main");
    if (!backup) return false;
    const int stepResult = sqlite3_backup_step(backup, -1);
    const int finishResult = sqlite3_backup_finish(backup);
    return stepResult == SQLITE_DONE && finishResult == SQLITE_OK && Exec("PRAGMA foreign_keys=ON;");
}

bool Database::HasCatalogSchema() {
    return CatalogSchema::Validate(connection_);
}

bool Database::Exec(const char* sql) {
    return connection_.Exec(sql);
}

bool Database::InitializeSchema() {
    if (!editable_) return false;
    return CatalogSchema::Initialize(connection_);
}

bool Database::BeginTransaction() {
    return editable_ && Exec("BEGIN TRANSACTION;");
}

bool Database::Commit() {
    return editable_ && Exec("COMMIT;");
}

bool Database::Rollback() {
    return editable_ && Exec("ROLLBACK;");
}

bool Database::SetCatalogDescription(const std::wstring& description) {
    if (!editable_) return false;
    SQLiteStatement statement(connection_.Raw(), "UPDATE catalog_metadata SET description=? WHERE id=1;");
    statement.BindText(1, wit::platform::ToUtf8(description));
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

wit::core::CatalogMetadata Database::GetCatalogMetadata() {
    wit::core::CatalogMetadata metadata;
    SQLiteStatement statement(connection_.Raw(), "SELECT description FROM catalog_metadata WHERE id=1;");
    if (sqlite3_step(statement.Raw()) == SQLITE_ROW) metadata.description = Text(statement.Raw(), 0);
    return metadata;
}

wit::core::CatalogSummary Database::GetCatalogSummary() const {
    wit::core::CatalogSummary summary;
    summary.catalogFileSize = connection_.Path().empty() ? 0 : wit::platform::FileSize(connection_.Path());
    if (!connection_.Raw()) return summary;
    SQLiteStatement statement(connection_.Raw(),
        "SELECT (SELECT COUNT(*) FROM disks),(SELECT COUNT(*) FROM files),(SELECT COUNT(*) FROM folders),"
        "COALESCE((SELECT SUM(total_capacity) FROM disks),0),"
        "COALESCE((SELECT SUM(CASE WHEN total_capacity > free_space THEN total_capacity-free_space ELSE 0 END) FROM disks),0);");
    if (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        summary.totalDisks = sqlite3_column_int64(statement.Raw(), 0);
        summary.totalFiles = sqlite3_column_int64(statement.Raw(), 1);
        summary.totalFolders = sqlite3_column_int64(statement.Raw(), 2);
        summary.totalStorageSpace = static_cast<std::uint64_t>(sqlite3_column_int64(statement.Raw(), 3));
        summary.totalUsedSpace = static_cast<std::uint64_t>(sqlite3_column_int64(statement.Raw(), 4));
    }
    return summary;
}

std::int64_t Database::CreateDiskGroup(const std::wstring& name) {
    if (!editable_) return 0;
    const auto now = wit::platform::NowUnixSeconds();
    SQLiteStatement statement(connection_.Raw(),
        "INSERT INTO disk_groups(name,created_at,updated_at) VALUES(?,?,?);");
    statement.BindText(1, wit::platform::ToUtf8(name));
    statement.BindInt64(2, now);
    statement.BindInt64(3, now);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE ? sqlite3_last_insert_rowid(connection_.Raw()) : 0;
}

std::vector<wit::core::DiskGroup> Database::GetDiskGroups() {
    std::vector<wit::core::DiskGroup> groups;
    SQLiteStatement statement(connection_.Raw(),
        "SELECT g.id,g.name,g.created_at,g.updated_at,COUNT(d.id) "
        "FROM disk_groups g LEFT JOIN disks d ON d.disk_group_id=g.id "
        "GROUP BY g.id,g.name,g.created_at,g.updated_at ORDER BY g.name COLLATE NOCASE,g.id;");
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::DiskGroup group;
        group.id = sqlite3_column_int64(statement.Raw(), 0);
        group.name = Text(statement.Raw(), 1);
        group.createdAt = sqlite3_column_int64(statement.Raw(), 2);
        group.updatedAt = sqlite3_column_int64(statement.Raw(), 3);
        group.totalDisks = sqlite3_column_int64(statement.Raw(), 4);
        groups.push_back(group);
    }
    return groups;
}

std::int64_t Database::AddDisk(const wit::core::Disk& disk) {
    if (!editable_) return 0;
    SQLiteStatement statement(connection_.Raw(),
        "INSERT INTO disks(disk_group_id,disk_name,disk_number,source_path,volume_label,total_capacity,free_space,cluster_size,"
        "serial_number,file_system,total_files,total_folders,added_at,updated_at,description,category,location,disk_type)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    if (disk.diskGroupId != 0) statement.BindInt64(1, disk.diskGroupId); else statement.BindNull(1);
    statement.BindText(2, wit::platform::ToUtf8(disk.diskName));
    statement.BindInt64(3, disk.diskNumber);
    statement.BindText(4, wit::platform::ToUtf8(disk.sourcePath));
    statement.BindText(5, wit::platform::ToUtf8(disk.volumeLabel));
    statement.BindInt64(6, static_cast<long long>(disk.totalCapacity));
    statement.BindInt64(7, static_cast<long long>(disk.freeSpace));
    statement.BindInt64(8, static_cast<long long>(disk.clusterSize));
    statement.BindText(9, wit::platform::ToUtf8(disk.serialNumber));
    statement.BindText(10, wit::platform::ToUtf8(disk.fileSystem));
    statement.BindInt64(11, disk.totalFiles);
    statement.BindInt64(12, disk.totalFolders);
    statement.BindInt64(13, disk.addedAt);
    statement.BindInt64(14, disk.updatedAt);
    statement.BindText(15, wit::platform::ToUtf8(disk.description));
    statement.BindText(16, wit::platform::ToUtf8(disk.category));
    statement.BindText(17, wit::platform::ToUtf8(disk.location));
    statement.BindText(18, wit::core::DiskTypeValue(disk.diskType));
    return sqlite3_step(statement.Raw()) == SQLITE_DONE ? sqlite3_last_insert_rowid(connection_.Raw()) : 0;
}

std::int64_t Database::FindDiskBySourcePath(const std::wstring& sourcePath, const std::wstring& originalLocation) {
    SQLiteStatement statement(connection_.Raw(),
        "SELECT id FROM disks WHERE source_path=? COLLATE NOCASE OR "
        "(? <> '' AND location=? COLLATE NOCASE) LIMIT 1;");
    statement.BindText(1, wit::platform::ToUtf8(sourcePath));
    statement.BindText(2, wit::platform::ToUtf8(originalLocation));
    statement.BindText(3, wit::platform::ToUtf8(originalLocation));
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int64(statement.Raw(), 0) : 0;
}

bool Database::DeleteContentForDisk(std::int64_t diskId) {
    if (!editable_) return false;
    SQLiteStatement statement(connection_.Raw(), "DELETE FROM folders WHERE disk_id=?;");
    statement.BindInt64(1, diskId);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

bool Database::UpdateDisk(const wit::core::Disk& disk) {
    if (!editable_) return false;
    SQLiteStatement statement(connection_.Raw(),
        "UPDATE disks SET disk_group_id=?,disk_name=?,disk_number=?,source_path=?,volume_label=?,total_capacity=?,free_space=?,cluster_size=?,"
        "serial_number=?,file_system=?,total_files=?,total_folders=?,updated_at=?,location=?,disk_type=? WHERE id=?;");
    if (disk.diskGroupId != 0) statement.BindInt64(1, disk.diskGroupId); else statement.BindNull(1);
    statement.BindText(2, wit::platform::ToUtf8(disk.diskName));
    statement.BindInt64(3, disk.diskNumber);
    statement.BindText(4, wit::platform::ToUtf8(disk.sourcePath));
    statement.BindText(5, wit::platform::ToUtf8(disk.volumeLabel));
    statement.BindInt64(6, static_cast<long long>(disk.totalCapacity));
    statement.BindInt64(7, static_cast<long long>(disk.freeSpace));
    statement.BindInt64(8, static_cast<long long>(disk.clusterSize));
    statement.BindText(9, wit::platform::ToUtf8(disk.serialNumber));
    statement.BindText(10, wit::platform::ToUtf8(disk.fileSystem));
    statement.BindInt64(11, disk.totalFiles);
    statement.BindInt64(12, disk.totalFolders);
    statement.BindInt64(13, disk.updatedAt);
    statement.BindText(14, wit::platform::ToUtf8(disk.location));
    statement.BindText(15, wit::core::DiskTypeValue(disk.diskType));
    statement.BindInt64(16, disk.id);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

bool Database::UpdateDiskScanStatistics(const wit::core::DiskScanStatistics& statistics) {
    if (!editable_) return false;
    SQLiteStatement statement(connection_.Raw(),
        "INSERT INTO disk_scan_statistics(disk_id,last_scanned_at,image_scanning_time_ms,imported_descriptions_count,"
        "calculated_file_crcs,scanned_archives,archive_files_count,archive_folders_count) VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(disk_id) DO UPDATE SET "
        "last_scanned_at=excluded.last_scanned_at,image_scanning_time_ms=excluded.image_scanning_time_ms,"
        "imported_descriptions_count=excluded.imported_descriptions_count,"
        "calculated_file_crcs=excluded.calculated_file_crcs,scanned_archives=excluded.scanned_archives,"
        "archive_files_count=excluded.archive_files_count,archive_folders_count=excluded.archive_folders_count;");
    statement.BindInt64(1, statistics.diskId);
    statement.BindInt64(2, statistics.lastScannedAt);
    statement.BindInt64(3, statistics.imageScanningTimeMs);
    statement.BindInt64(4, statistics.importedDescriptionsCount);
    statement.BindInt64(5, statistics.calculatedFileCrcs ? 1 : 0);
    statement.BindInt64(6, statistics.scannedArchives);
    statement.BindInt64(7, statistics.archiveFilesCount);
    statement.BindInt64(8, statistics.archiveFoldersCount);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

int Database::GetDiskCount() {
    SQLiteStatement statement(connection_.Raw(), "SELECT COUNT(*) FROM disks;");
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::Disk> Database::GetDisksPage(int offset, int limit) {
    std::vector<wit::core::Disk> disks;
    SQLiteStatement statement(connection_.Raw(),
        "SELECT id,disk_group_id,disk_name,disk_number,source_path,total_capacity,free_space,updated_at,"
        "description,category,location,disk_type FROM disks "
        "ORDER BY disk_name COLLATE NOCASE,id LIMIT ? OFFSET ?;");
    statement.BindInt64(1, limit);
    statement.BindInt64(2, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::Disk disk;
        PopulateDisk(disk, statement.Raw());
        disks.push_back(disk);
    }
    return disks;
}

std::int64_t Database::InsertFolder(const wit::core::FolderEntry& folder) {
    if (!editable_) return 0;
    SQLiteStatement statement(connection_.Raw(),
        "INSERT INTO folders(disk_id,parent_folder_id,path,name,created_at,modified_at,accessed_at,attributes,content_size,entry_type)"
        " VALUES(?,?,?,?,?,?,?,?,?,?);");
    statement.BindInt64(1, folder.diskId);
    if (folder.hasParent) statement.BindInt64(2, folder.parentFolderId); else statement.BindNull(2);
    statement.BindText(3, wit::platform::ToUtf8(folder.path));
    statement.BindText(4, wit::platform::ToUtf8(folder.name));
    statement.BindInt64(5, folder.createdAt);
    statement.BindInt64(6, folder.modifiedAt);
    statement.BindInt64(7, folder.accessedAt);
    statement.BindInt64(8, folder.attributes);
    statement.BindInt64(9, static_cast<long long>(folder.contentSize));
    statement.BindText(10, wit::core::FolderEntryTypeValue(folder.entryType));
    return sqlite3_step(statement.Raw()) == SQLITE_DONE ? sqlite3_last_insert_rowid(connection_.Raw()) : 0;
}

bool Database::UpdateFolderContentSize(std::int64_t folderId, std::uint64_t contentSize) {
    if (!editable_) return false;
    SQLiteStatement statement(connection_.Raw(), "UPDATE folders SET content_size=? WHERE id=?;");
    statement.BindInt64(1, static_cast<long long>(contentSize));
    statement.BindInt64(2, folderId);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE && sqlite3_changes(connection_.Raw()) == 1;
}

bool Database::InsertFile(const wit::core::FileEntry& file) {
    if (!editable_) return false;
    SQLiteStatement statement(connection_.Raw(),
        "INSERT INTO files(disk_id,folder_id,name,description,extension,crc,size,created_at,modified_at,accessed_at,attributes)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?);");
    statement.BindInt64(1, file.catalogId);
    statement.BindInt64(2, file.folderId);
    statement.BindText(3, wit::platform::ToUtf8(file.name));
    statement.BindText(4, wit::platform::ToUtf8(file.description));
    statement.BindText(5, wit::platform::ToUtf8(file.extension));
    if (file.crc) statement.BindText(6, wit::platform::ToUtf8(*file.crc)); else statement.BindNull(6);
    statement.BindInt64(7, static_cast<long long>(file.size));
    statement.BindInt64(8, file.createdAt);
    statement.BindInt64(9, file.modifiedAtValue);
    statement.BindInt64(10, file.accessedAt);
    statement.BindInt64(11, file.attributes);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

std::vector<wit::core::Catalog> Database::GetCatalogs() {
    std::vector<wit::core::Catalog> catalogs;
    SQLiteStatement statement(connection_.Raw(),
        "SELECT id,disk_name,source_path,added_at,total_files,total_folders FROM disks "
        "WHERE disk_group_id IS NULL ORDER BY id DESC;");
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::Catalog catalog;
        catalog.id = sqlite3_column_int64(statement.Raw(), 0);
        catalog.name = Text(statement.Raw(), 1);
        catalog.rootPath = Text(statement.Raw(), 2);
        catalog.addedAt = sqlite3_column_int64(statement.Raw(), 3);
        catalog.totalFiles = sqlite3_column_int64(statement.Raw(), 4);
        catalog.totalFolders = sqlite3_column_int64(statement.Raw(), 5);
        catalogs.push_back(catalog);
    }
    return catalogs;
}

int Database::GetBrowserRootItemCount(const wit::core::BrowserLocation& location) {
    SqliteBrowserRepository browser(connection_.Raw());
    return browser.GetBrowserRootItemCount(location);
}

std::vector<wit::core::BrowserItem> Database::GetBrowserRootItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit) {
    SqliteBrowserRepository browser(connection_.Raw());
    return browser.GetBrowserRootItemsPage(location, offset, limit);
}

int Database::GetBrowserItemCount(const wit::core::BrowserLocation& location) {
    SqliteBrowserRepository browser(connection_.Raw());
    return browser.GetBrowserItemCount(location);
}

std::vector<wit::core::FileEntry> Database::GetBrowserItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit) {
    SqliteBrowserRepository browser(connection_.Raw());
    return browser.GetBrowserItemsPage(location, offset, limit);
}

bool Database::HasChildFolders(std::int64_t sourceId, const std::wstring& parentPath) {
    SqliteBrowserRepository browser(connection_.Raw());
    return browser.HasChildFolders(sourceId, parentPath);
}

std::vector<wit::core::FileEntry> Database::GetChildFolders(
    std::int64_t sourceId, const std::wstring& parentPath) {
    SqliteBrowserRepository browser(connection_.Raw());
    return browser.GetChildFolders(sourceId, parentPath);
}

int Database::GetItemSearchCount(const std::wstring& nameTerm) {
    wit::search::SqliteSearchExecutor search(connection_.Raw());
    return search.CountByName(nameTerm);
}

std::vector<wit::core::FileEntry> Database::GetItemSearchPage(const std::wstring& nameTerm, int offset, int limit) {
    wit::search::SqliteSearchExecutor search(connection_.Raw());
    return search.PageByName(nameTerm, offset, limit);
}
}
