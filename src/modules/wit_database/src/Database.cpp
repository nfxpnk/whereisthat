#include "wit_database/Database.h"
#include "wit_database/SQLiteStatement.h"
#include <wit_infra/VolumeInfo.h>
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"
#include <Windows.h>
#include <algorithm>
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
    entry.modifiedAtValue = sqlite3_column_int64(stmt, 6);
    entry.modifiedAt = wit::platform::FormatUnixTimestamp(entry.modifiedAtValue);
    entry.attributes = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
    entry.isDirectory = sqlite3_column_int(stmt, 8) != 0;
    entry.isArchive = Text(stmt, 9) == L"archive";
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
    disk.diskName = Text(stmt, 1);
    disk.diskNumber = sqlite3_column_int64(stmt, 2);
    disk.sourcePath = Text(stmt, 3);
    disk.totalCapacity = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
    disk.freeSpace = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
    disk.updatedAt = sqlite3_column_int64(stmt, 6);
    disk.description = Text(stmt, 7);
    disk.category = Text(stmt, 8);
    disk.location = Text(stmt, 9);
    disk.diskType = DiskTypeFromText(Text(stmt, 10));
}
}

Database::~Database() {
    Close();
}

Database::Database(Database&& other) noexcept
    : db_(std::exchange(other.db_, nullptr)), editable_(std::exchange(other.editable_, false)),
      path_(std::move(other.path_)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        Close();
        db_ = std::exchange(other.db_, nullptr);
        editable_ = std::exchange(other.editable_, false);
        path_ = std::move(other.path_);
    }
    return *this;
}

void Database::Close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    editable_ = false;
    path_.clear();
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
    if (sqlite3_open_v2(wit::platform::ToUtf8(path).c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        Close();
        return false;
    }
    path_ = path;
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
    if (!source.db_) return false;
    Close();
    if (sqlite3_open_v2(":memory:", &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        Close();
        return false;
    }
    editable_ = true;
    auto* backup = sqlite3_backup_init(db_, "main", source.db_, "main");
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
    if (!db_ || !editable_ || !source.db_) return false;
    auto* backup = sqlite3_backup_init(db_, "main", source.db_, "main");
    if (!backup) return false;
    const int stepResult = sqlite3_backup_step(backup, -1);
    const int finishResult = sqlite3_backup_finish(backup);
    return stepResult == SQLITE_DONE && finishResult == SQLITE_OK && Exec("PRAGMA foreign_keys=ON;");
}

bool Database::HasCatalogSchema() {
    return TableHasColumn(db_, "catalog_metadata", "description") &&
        TableHasColumn(db_, "disks", "disk_name") &&
        TableHasColumn(db_, "disks", "source_path") &&
        TableHasColumn(db_, "disks", "disk_type") &&
        TableHasColumn(db_, "disk_scan_statistics", "last_scanned_at") &&
        TableHasColumn(db_, "disk_scan_statistics", "scanned_archives") &&
        TableHasColumn(db_, "disk_scan_statistics", "archive_files_count") &&
        TableHasColumn(db_, "disk_scan_statistics", "archive_folders_count") &&
        TableHasColumn(db_, "folders", "parent_folder_id") &&
        TableHasColumn(db_, "folders", "path") &&
        TableHasColumn(db_, "folders", "content_size") &&
        TableHasColumn(db_, "folders", "entry_type") &&
        TableHasColumn(db_, "files", "folder_id") &&
        TableHasColumn(db_, "files", "extension") &&
        TableHasColumn(db_, "files", "crc") &&
        TableHasColumn(db_, "files", "accessed_at");
}

bool Database::Exec(const char* sql) {
    return sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Database::InitializeSchema() {
    if (!editable_) return false;
    return Exec("PRAGMA foreign_keys=ON;") && Exec("PRAGMA journal_mode=WAL;") && Exec("PRAGMA synchronous=NORMAL;") &&
        Exec("CREATE TABLE IF NOT EXISTS catalog_metadata (id INTEGER PRIMARY KEY CHECK (id=1),description TEXT NOT NULL DEFAULT '');") &&
        Exec("CREATE TABLE IF NOT EXISTS disks (id INTEGER PRIMARY KEY AUTOINCREMENT,disk_name TEXT NOT NULL,disk_number INTEGER NOT NULL DEFAULT 0,source_path TEXT NOT NULL COLLATE NOCASE UNIQUE,volume_label TEXT NOT NULL DEFAULT '',total_capacity INTEGER NOT NULL DEFAULT 0,free_space INTEGER NOT NULL DEFAULT 0,cluster_size INTEGER NOT NULL DEFAULT 0,serial_number TEXT NOT NULL DEFAULT '',file_system TEXT NOT NULL DEFAULT '',total_files INTEGER NOT NULL DEFAULT 0,total_folders INTEGER NOT NULL DEFAULT 0,added_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,description TEXT NOT NULL DEFAULT '',category TEXT NOT NULL DEFAULT '',location TEXT NOT NULL DEFAULT '',disk_type TEXT NOT NULL CHECK (disk_type IN ('CD','DVD','BluRay','HardDisk','SolidStateDisk','RemovableUSB','VirtualDisk','Other')));") &&
        Exec("CREATE TABLE IF NOT EXISTS disk_scan_statistics (disk_id INTEGER PRIMARY KEY,last_scanned_at INTEGER NOT NULL,image_scanning_time_ms INTEGER NOT NULL DEFAULT 0,imported_descriptions_count INTEGER NOT NULL DEFAULT 0,calculated_file_crcs INTEGER NOT NULL DEFAULT 0 CHECK (calculated_file_crcs IN (0,1)),scanned_archives INTEGER NOT NULL DEFAULT 0 CHECK (scanned_archives >= 0),archive_files_count INTEGER NOT NULL DEFAULT 0 CHECK (archive_files_count >= 0),archive_folders_count INTEGER NOT NULL DEFAULT 0 CHECK (archive_folders_count >= 0),FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE);") &&
        Exec("CREATE TABLE IF NOT EXISTS folders (id INTEGER PRIMARY KEY AUTOINCREMENT,disk_id INTEGER NOT NULL,parent_folder_id INTEGER,path TEXT NOT NULL,name TEXT NOT NULL,created_at INTEGER NOT NULL,modified_at INTEGER NOT NULL,accessed_at INTEGER NOT NULL,attributes INTEGER NOT NULL DEFAULT 0,content_size INTEGER NOT NULL DEFAULT 0,entry_type TEXT NOT NULL DEFAULT 'directory' CHECK (entry_type IN ('directory','archive')),FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE,FOREIGN KEY (parent_folder_id) REFERENCES folders(id) ON DELETE CASCADE);") &&
        Exec("CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY AUTOINCREMENT,disk_id INTEGER NOT NULL,folder_id INTEGER NOT NULL,name TEXT NOT NULL,description TEXT NOT NULL DEFAULT '',extension TEXT NOT NULL DEFAULT '',crc TEXT,size INTEGER NOT NULL,created_at INTEGER NOT NULL,modified_at INTEGER NOT NULL,accessed_at INTEGER NOT NULL,attributes INTEGER NOT NULL DEFAULT 0,FOREIGN KEY (disk_id) REFERENCES disks(id) ON DELETE CASCADE,FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE CASCADE);") &&
        Exec("INSERT OR IGNORE INTO catalog_metadata(id,description) VALUES(1,'');") &&
        Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_disks_source_path ON disks(source_path COLLATE NOCASE);") &&
        Exec("CREATE INDEX IF NOT EXISTS idx_folders_parent ON folders(disk_id,parent_folder_id);") &&
        Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_folders_disk_path ON folders(disk_id,path COLLATE NOCASE);") &&
        Exec("CREATE INDEX IF NOT EXISTS idx_files_folder ON files(folder_id);") &&
        Exec("CREATE INDEX IF NOT EXISTS idx_files_disk_name ON files(disk_id,name);") &&
        Exec("CREATE INDEX IF NOT EXISTS idx_files_extension ON files(extension);");
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
    SQLiteStatement statement(db_, "UPDATE catalog_metadata SET description=? WHERE id=1;");
    statement.BindText(1, wit::platform::ToUtf8(description));
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

wit::core::CatalogMetadata Database::GetCatalogMetadata() {
    wit::core::CatalogMetadata metadata;
    SQLiteStatement statement(db_, "SELECT description FROM catalog_metadata WHERE id=1;");
    if (sqlite3_step(statement.Raw()) == SQLITE_ROW) metadata.description = Text(statement.Raw(), 0);
    return metadata;
}

wit::core::CatalogSummary Database::GetCatalogSummary() const {
    wit::core::CatalogSummary summary;
    summary.catalogFileSize = path_.empty() ? 0 : wit::platform::FileSize(path_);
    if (!db_) return summary;
    SQLiteStatement statement(db_,
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

std::int64_t Database::AddDisk(const wit::core::Disk& disk) {
    if (!editable_) return 0;
    SQLiteStatement statement(db_,
        "INSERT INTO disks(disk_name,disk_number,source_path,volume_label,total_capacity,free_space,cluster_size,"
        "serial_number,file_system,total_files,total_folders,added_at,updated_at,description,category,location,disk_type)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    statement.BindText(1, wit::platform::ToUtf8(disk.diskName));
    statement.BindInt64(2, disk.diskNumber);
    statement.BindText(3, wit::platform::ToUtf8(disk.sourcePath));
    statement.BindText(4, wit::platform::ToUtf8(disk.volumeLabel));
    statement.BindInt64(5, static_cast<long long>(disk.totalCapacity));
    statement.BindInt64(6, static_cast<long long>(disk.freeSpace));
    statement.BindInt64(7, static_cast<long long>(disk.clusterSize));
    statement.BindText(8, wit::platform::ToUtf8(disk.serialNumber));
    statement.BindText(9, wit::platform::ToUtf8(disk.fileSystem));
    statement.BindInt64(10, disk.totalFiles);
    statement.BindInt64(11, disk.totalFolders);
    statement.BindInt64(12, disk.addedAt);
    statement.BindInt64(13, disk.updatedAt);
    statement.BindText(14, wit::platform::ToUtf8(disk.description));
    statement.BindText(15, wit::platform::ToUtf8(disk.category));
    statement.BindText(16, wit::platform::ToUtf8(disk.location));
    statement.BindText(17, wit::core::DiskTypeValue(disk.diskType));
    return sqlite3_step(statement.Raw()) == SQLITE_DONE ? sqlite3_last_insert_rowid(db_) : 0;
}

std::int64_t Database::FindDiskBySourcePath(const std::wstring& sourcePath, const std::wstring& originalLocation) {
    SQLiteStatement statement(db_,
        "SELECT id FROM disks WHERE source_path=? COLLATE NOCASE OR "
        "(? <> '' AND location=? COLLATE NOCASE) LIMIT 1;");
    statement.BindText(1, wit::platform::ToUtf8(sourcePath));
    statement.BindText(2, wit::platform::ToUtf8(originalLocation));
    statement.BindText(3, wit::platform::ToUtf8(originalLocation));
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int64(statement.Raw(), 0) : 0;
}

bool Database::DeleteContentForDisk(std::int64_t diskId) {
    if (!editable_) return false;
    SQLiteStatement statement(db_, "DELETE FROM folders WHERE disk_id=?;");
    statement.BindInt64(1, diskId);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

bool Database::UpdateDisk(const wit::core::Disk& disk) {
    if (!editable_) return false;
    SQLiteStatement statement(db_,
        "UPDATE disks SET disk_name=?,disk_number=?,source_path=?,volume_label=?,total_capacity=?,free_space=?,cluster_size=?,"
        "serial_number=?,file_system=?,total_files=?,total_folders=?,updated_at=?,location=?,disk_type=? WHERE id=?;");
    statement.BindText(1, wit::platform::ToUtf8(disk.diskName));
    statement.BindInt64(2, disk.diskNumber);
    statement.BindText(3, wit::platform::ToUtf8(disk.sourcePath));
    statement.BindText(4, wit::platform::ToUtf8(disk.volumeLabel));
    statement.BindInt64(5, static_cast<long long>(disk.totalCapacity));
    statement.BindInt64(6, static_cast<long long>(disk.freeSpace));
    statement.BindInt64(7, static_cast<long long>(disk.clusterSize));
    statement.BindText(8, wit::platform::ToUtf8(disk.serialNumber));
    statement.BindText(9, wit::platform::ToUtf8(disk.fileSystem));
    statement.BindInt64(10, disk.totalFiles);
    statement.BindInt64(11, disk.totalFolders);
    statement.BindInt64(12, disk.updatedAt);
    statement.BindText(13, wit::platform::ToUtf8(disk.location));
    statement.BindText(14, wit::core::DiskTypeValue(disk.diskType));
    statement.BindInt64(15, disk.id);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE;
}

bool Database::UpdateDiskScanStatistics(const wit::core::DiskScanStatistics& statistics) {
    if (!editable_) return false;
    SQLiteStatement statement(db_,
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
    SQLiteStatement statement(db_, "SELECT COUNT(*) FROM disks;");
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::Disk> Database::GetDisksPage(int offset, int limit) {
    std::vector<wit::core::Disk> disks;
    SQLiteStatement statement(db_,
        "SELECT id,disk_name,disk_number,source_path,total_capacity,free_space,updated_at,"
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
    SQLiteStatement statement(db_,
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
    return sqlite3_step(statement.Raw()) == SQLITE_DONE ? sqlite3_last_insert_rowid(db_) : 0;
}

bool Database::UpdateFolderContentSize(std::int64_t folderId, std::uint64_t contentSize) {
    if (!editable_) return false;
    SQLiteStatement statement(db_, "UPDATE folders SET content_size=? WHERE id=?;");
    statement.BindInt64(1, static_cast<long long>(contentSize));
    statement.BindInt64(2, folderId);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE && sqlite3_changes(db_) == 1;
}

bool Database::InsertFile(const wit::core::FileEntry& file) {
    if (!editable_) return false;
    SQLiteStatement statement(db_,
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
    SQLiteStatement statement(db_,
        "SELECT id,disk_name,source_path,added_at,total_files,total_folders FROM disks ORDER BY id DESC;");
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

int Database::GetBrowserItemCount(const wit::core::BrowserLocation& location) {
    if (location.isRoot) {
        SQLiteStatement statement(db_, "SELECT COUNT(*) FROM disks;");
        return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
    }
    SQLiteStatement statement(db_,
        "WITH parent(id) AS (SELECT id FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT (SELECT COUNT(*) FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=?) + "
        "(SELECT COUNT(*) FROM files f JOIN parent p ON f.folder_id=p.id);");
    statement.BindInt64(1, location.sourceId);
    statement.BindText(2, wit::platform::ToUtf8(location.path));
    statement.BindInt64(3, location.sourceId);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::FileEntry> Database::GetBrowserItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    if (location.isRoot) {
        SQLiteStatement statement(db_,
            "SELECT id,id,source_path,disk_name,'',0,updated_at,0,1,'directory' FROM disks ORDER BY disk_name LIMIT ? OFFSET ?;");
        statement.BindInt64(1, limit);
        statement.BindInt64(2, offset);
        while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
            wit::core::FileEntry file;
            PopulateDisplayEntry(file, statement.Raw());
            files.push_back(file);
        }
        return files;
    }
    SQLiteStatement statement(db_,
        "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT id,disk_id,parent_path,name,extension,size,modified_at,attributes,is_directory,entry_type FROM ("
        "SELECT c.id,c.disk_id,p.path AS parent_path,c.name,'' AS extension,c.content_size AS size,c.modified_at,c.attributes,1 AS is_directory,c.entry_type "
        "FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=? "
        "UNION ALL SELECT f.id,f.disk_id,p.path AS parent_path,f.name,f.extension,f.size,f.modified_at,f.attributes,0 AS is_directory,'file' AS entry_type "
        "FROM files f JOIN parent p ON f.folder_id=p.id)"
        " ORDER BY is_directory DESC,name LIMIT ? OFFSET ?;");
    statement.BindInt64(1, location.sourceId);
    statement.BindText(2, wit::platform::ToUtf8(location.path));
    statement.BindInt64(3, location.sourceId);
    statement.BindInt64(4, limit);
    statement.BindInt64(5, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry file;
        PopulateDisplayEntry(file, statement.Raw());
        files.push_back(file);
    }
    return files;
}

bool Database::HasChildFolders(std::int64_t sourceId, const std::wstring& parentPath) {
    SQLiteStatement statement(db_,
        "WITH parent(id) AS (SELECT id FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT EXISTS(SELECT 1 FROM folders c JOIN parent p ON c.parent_folder_id=p.id WHERE c.disk_id=?);");
    statement.BindInt64(1, sourceId);
    statement.BindText(2, wit::platform::ToUtf8(parentPath));
    statement.BindInt64(3, sourceId);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW && sqlite3_column_int(statement.Raw(), 0) != 0;
}

std::vector<wit::core::FileEntry> Database::GetChildFolders(
    std::int64_t sourceId, const std::wstring& parentPath) {
    std::vector<wit::core::FileEntry> folders;
    SQLiteStatement statement(db_,
        "WITH parent(id,path) AS (SELECT id,path FROM folders WHERE disk_id=? AND path=? COLLATE NOCASE) "
        "SELECT c.id,c.disk_id,p.path,c.name,'',c.content_size,c.modified_at,c.attributes,1,c.entry_type "
        "FROM folders c JOIN parent p ON c.parent_folder_id=p.id "
        "WHERE c.disk_id=? ORDER BY c.name;");
    statement.BindInt64(1, sourceId);
    statement.BindText(2, wit::platform::ToUtf8(parentPath));
    statement.BindInt64(3, sourceId);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry folder;
        PopulateDisplayEntry(folder, statement.Raw());
        folders.push_back(folder);
    }
    return folders;
}

int Database::GetItemSearchCount(const std::wstring& nameTerm) {
    SQLiteStatement statement(db_,
        "SELECT (SELECT COUNT(*) FROM files WHERE name LIKE ? ESCAPE '\\') + "
        "(SELECT COUNT(*) FROM folders WHERE name LIKE ? ESCAPE '\\');");
    const auto pattern = ItemNameLikePattern(nameTerm);
    statement.BindText(1, pattern);
    statement.BindText(2, pattern);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW ? sqlite3_column_int(statement.Raw(), 0) : 0;
}

std::vector<wit::core::FileEntry> Database::GetItemSearchPage(const std::wstring& nameTerm, int offset, int limit) {
    std::vector<wit::core::FileEntry> files;
    SQLiteStatement statement(db_,
        "SELECT id,disk_id,parent_path,name,extension,size,modified_at,attributes,is_directory,entry_type FROM ("
        "SELECT f.id,f.disk_id,p.path AS parent_path,f.name,f.extension,f.size,f.modified_at,f.attributes,0 AS is_directory,'file' AS entry_type "
        "FROM files f JOIN folders p ON f.folder_id=p.id WHERE f.name LIKE ? ESCAPE '\\' "
        "UNION ALL SELECT c.id,c.disk_id,COALESCE(p.path,''),c.name,'',c.content_size,c.modified_at,c.attributes,1,c.entry_type "
        "FROM folders c LEFT JOIN folders p ON c.parent_folder_id=p.id WHERE c.name LIKE ? ESCAPE '\\') "
        "ORDER BY is_directory DESC,name,parent_path LIMIT ? OFFSET ?;");
    const auto pattern = ItemNameLikePattern(nameTerm);
    statement.BindText(1, pattern);
    statement.BindText(2, pattern);
    statement.BindInt64(3, limit);
    statement.BindInt64(4, offset);
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::FileEntry file;
        PopulateDisplayEntry(file, statement.Raw());
        files.push_back(file);
    }
    return files;
}
}

