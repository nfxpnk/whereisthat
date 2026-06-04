#include "wit_database/CatalogSchema.h"
#include "wit_database/Database.h"
#include "wit_database/SQLiteStatement.h"
#include <wit_infra/VolumeInfo.h>
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
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

bool BackupDatabase(sqlite3* destination, sqlite3* source) {
    auto* backup = sqlite3_backup_init(destination, "main", source, "main");
    if (!backup) return false;

    constexpr int kPageChunkSize = 128;
    constexpr int kMaxBusyRetries = 50;
    constexpr auto kBusyRetryDelay = std::chrono::milliseconds(20);

    bool copied = false;
    int busyRetries = 0;
    for (;;) {
        const int stepResult = sqlite3_backup_step(backup, kPageChunkSize);
        if (stepResult == SQLITE_DONE) {
            copied = true;
            break;
        }
        if (stepResult == SQLITE_OK) {
            busyRetries = 0;
            continue;
        }
        if ((stepResult == SQLITE_BUSY || stepResult == SQLITE_LOCKED) && busyRetries < kMaxBusyRetries) {
            ++busyRetries;
            std::this_thread::sleep_for(kBusyRetryDelay);
            continue;
        }
        break;
    }

    const int finishResult = sqlite3_backup_finish(backup);
    return copied && finishResult == SQLITE_OK;
}

std::wstring MakeTempCatalogPath(const std::wstring& catalogPath) {
    static std::atomic<unsigned long> counter{0};
    const auto sequence = counter.fetch_add(1, std::memory_order_relaxed);
    return catalogPath + L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(sequence);
}

void DeleteCatalogFileSet(const std::wstring& path) {
    DeleteFileW(path.c_str());
    DeleteFileW((path + L"-journal").c_str());
    DeleteFileW((path + L"-wal").c_str());
    DeleteFileW((path + L"-shm").c_str());
}

bool IntegrityCheckOk(SqliteConnection& connection) {
    sqlite3_stmt* statement{};
    if (sqlite3_prepare_v2(connection.Raw(), "PRAGMA integrity_check;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    bool ok = false;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const auto* result = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        ok = result && std::string(result) == "ok" && sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement);
    return ok;
}

bool PragmaReturns(SqliteConnection& connection, const char* sql, const char* expectedText) {
    sqlite3_stmt* statement{};
    if (sqlite3_prepare_v2(connection.Raw(), sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    bool ok = false;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const auto* result = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        ok = result && std::string(result) == expectedText;
    }
    sqlite3_finalize(statement);
    return ok;
}

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

bool UpgradeCatalogSchema(SqliteConnection& connection) {
    if (TableHasColumn(connection.Raw(), "disk_groups", "parent_group_id")) return true;
    if (!TableHasColumn(connection.Raw(), "disk_groups", "name") ||
        !TableHasColumn(connection.Raw(), "disk_groups", "updated_at")) {
        return true;
    }
    return connection.Exec("ALTER TABLE disk_groups ADD COLUMN parent_group_id INTEGER;");
}

bool DiskGroupExists(sqlite3* db, std::int64_t diskGroupId) {
    SQLiteStatement statement(db, "SELECT 1 FROM disk_groups WHERE id=?;");
    statement.BindInt64(1, diskGroupId);
    return sqlite3_step(statement.Raw()) == SQLITE_ROW;
}

bool CatalogSidecarsAbsent(const std::wstring& path) {
    return GetFileAttributesW((path + L"-journal").c_str()) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((path + L"-wal").c_str()) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((path + L"-shm").c_str()) == INVALID_FILE_ATTRIBUTES;
}

bool PrepareSingleFileCatalog(SqliteConnection& connection) {
    return connection.Exec("PRAGMA wal_checkpoint(TRUNCATE);") &&
        PragmaReturns(connection, "PRAGMA journal_mode=DELETE;", "delete");
}

bool VerifyCatalog(SqliteConnection& connection) {
    return connection.Exec("PRAGMA foreign_keys=ON;") &&
        IntegrityCheckOk(connection) &&
        CatalogSchema::Validate(connection);
}

bool ReplaceCatalogFile(const std::wstring& catalogPath, const std::wstring& replacementPath,
    const std::wstring& backupPath) {
    DeleteCatalogFileSet(backupPath);
    if (ReplaceFileW(catalogPath.c_str(), replacementPath.c_str(), backupPath.c_str(),
        REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
        return true;
    }
    if (GetFileAttributesW(catalogPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return MoveFileExW(replacementPath.c_str(), catalogPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!MoveFileExW(catalogPath.c_str(), backupPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return false;
    }
    if (MoveFileExW(replacementPath.c_str(), catalogPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    const bool restored = MoveFileExW(backupPath.c_str(), catalogPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    (void)restored;
    return false;
}

bool RestoreCatalogFile(const std::wstring& catalogPath, const std::wstring& backupPath) {
    if (backupPath.empty() || GetFileAttributesW(backupPath.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    DeleteFileW((catalogPath + L"-journal").c_str());
    DeleteFileW((catalogPath + L"-wal").c_str());
    DeleteFileW((catalogPath + L"-shm").c_str());
    return MoveFileExW(backupPath.c_str(), catalogPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool ApplyEditableCatalogPragmas(SqliteConnection& connection) {
    return connection.Exec("PRAGMA foreign_keys=ON;") &&
        connection.Exec("PRAGMA journal_mode=WAL;") &&
        connection.Exec("PRAGMA synchronous=NORMAL;");
}
}

Database::~Database() {
    Close();
}

Database::Database(Database&& other) noexcept
    : connection_(std::move(other.connection_)), editable_(std::exchange(other.editable_, false)) {
    RebindRepositories();
    other.RebindRepositories();
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        Close();
        connection_ = std::move(other.connection_);
        editable_ = std::exchange(other.editable_, false);
        RebindRepositories();
        other.RebindRepositories();
    }
    return *this;
}

void Database::Close() {
    connection_.Close();
    editable_ = false;
    RebindRepositories();
}

void Database::RebindRepositories() {
    browserRepository_.SetDatabase(connection_.Raw());
    searchRepository_.SetDatabase(connection_.Raw());
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
    RebindRepositories();
    editable_ = !readOnly;
    if (requireExistingSchema && !readOnly && !UpgradeCatalogSchema(connection_)) {
        Close();
        return false;
    }
    if (requireExistingSchema && !HasCatalogSchema()) {
        Close();
        return false;
    }
    if (readOnly) return Exec("PRAGMA foreign_keys=ON;");
    if (!requireExistingSchema && InitializeSchema()) return true;
    if (requireExistingSchema && ApplyEditableCatalogPragmas(connection_)) return true;
    Close();
    return false;
}

bool Database::CreateWorkingCopy(const Database& source) {
    if (!source.connection_.IsOpen()) return false;
    Close();
    if (!connection_.OpenMemory()) return false;
    RebindRepositories();
    editable_ = true;
    const bool success = BackupDatabase(connection_.Raw(), source.connection_.Raw()) && Exec("PRAGMA foreign_keys=ON;");
    if (!success) Close();
    return success;
}

// Manual smoke checklist when changing this path:
// 1. Save pending edits into a normal editable catalog and reopen it.
// 2. Make the catalog read-only or locked by another process; save must fail and leave pending edits.
// 3. Break temp verification or replacement under a debugger; the original catalog must still open.
bool Database::SaveCatalogDataFrom(const Database& source) {
    if (!connection_.IsOpen() || !editable_ || !source.connection_.IsOpen()) return false;
    const std::wstring catalogPath = connection_.Path();
    if (catalogPath.empty()) return false;

    std::wstring tempPath;
    bool tempCreated = false;
    for (int attempt = 0; attempt < 16; ++attempt) {
        tempPath = MakeTempCatalogPath(catalogPath);
        if (GetFileAttributesW(tempPath.c_str()) != INVALID_FILE_ATTRIBUTES) continue;
        SqliteConnection tempConnection;
        if (!tempConnection.Open(tempPath, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE)) {
            DeleteCatalogFileSet(tempPath);
            continue;
        }
        tempCreated = true;
        if (!BackupDatabase(tempConnection.Raw(), source.connection_.Raw()) || !VerifyCatalog(tempConnection)) {
            tempConnection.Close();
            DeleteCatalogFileSet(tempPath);
            return false;
        }
        if (!PrepareSingleFileCatalog(tempConnection)) {
            tempConnection.Close();
            DeleteCatalogFileSet(tempPath);
            return false;
        }
        tempConnection.Close();
        if (!CatalogSidecarsAbsent(tempPath)) {
            DeleteCatalogFileSet(tempPath);
            return false;
        }
        break;
    }
    if (!tempCreated) return false;

    if (!PrepareSingleFileCatalog(connection_)) {
        DeleteCatalogFileSet(tempPath);
        return false;
    }
    const std::wstring backupPath = MakeTempCatalogPath(catalogPath + L".backup");
    Close();
    if (!CatalogSidecarsAbsent(catalogPath)) {
        DeleteCatalogFileSet(tempPath);
        const bool reopened = OpenInternal(catalogPath, true) || OpenInternal(catalogPath, true, true);
        (void)reopened;
        return false;
    }
    if (!ReplaceCatalogFile(catalogPath, tempPath, backupPath)) {
        DeleteCatalogFileSet(tempPath);
        DeleteCatalogFileSet(backupPath);
        const bool reopened = OpenInternal(catalogPath, true) || OpenInternal(catalogPath, true, true);
        (void)reopened;
        return false;
    }

    if (OpenInternal(catalogPath, true)) {
        DeleteCatalogFileSet(backupPath);
        return true;
    }
    Close();
    const bool restored = RestoreCatalogFile(catalogPath, backupPath);
    const bool reopenedOriginal = restored && (OpenInternal(catalogPath, true) || OpenInternal(catalogPath, true, true));
    (void)reopenedOriginal;
    return false;
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
        "INSERT INTO disk_groups(parent_group_id,name,created_at,updated_at) VALUES(NULL,?,?,?);");
    statement.BindText(1, wit::platform::ToUtf8(name));
    statement.BindInt64(2, now);
    statement.BindInt64(3, now);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE ? sqlite3_last_insert_rowid(connection_.Raw()) : 0;
}

std::vector<wit::core::DiskGroup> Database::GetDiskGroups() {
    std::vector<wit::core::DiskGroup> groups;
    SQLiteStatement statement(connection_.Raw(),
        "SELECT g.id,g.parent_group_id,g.name,g.created_at,g.updated_at,COUNT(d.id) "
        "FROM disk_groups g LEFT JOIN disks d ON d.disk_group_id=g.id "
        "GROUP BY g.id,g.parent_group_id,g.name,g.created_at,g.updated_at ORDER BY g.name COLLATE NOCASE,g.id;");
    while (sqlite3_step(statement.Raw()) == SQLITE_ROW) {
        wit::core::DiskGroup group;
        group.id = sqlite3_column_int64(statement.Raw(), 0);
        group.parentGroupId = sqlite3_column_type(statement.Raw(), 1) == SQLITE_NULL
            ? 0 : sqlite3_column_int64(statement.Raw(), 1);
        group.name = Text(statement.Raw(), 2);
        group.createdAt = sqlite3_column_int64(statement.Raw(), 3);
        group.updatedAt = sqlite3_column_int64(statement.Raw(), 4);
        group.totalDisks = sqlite3_column_int64(statement.Raw(), 5);
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

bool Database::MoveDiskToGroup(std::int64_t diskId, std::int64_t diskGroupId) {
    if (!editable_ || diskId == 0) return false;
    if (diskGroupId != 0 && !DiskGroupExists(connection_.Raw(), diskGroupId)) return false;
    SQLiteStatement statement(connection_.Raw(), "UPDATE disks SET disk_group_id=? WHERE id=?;");
    if (diskGroupId != 0) statement.BindInt64(1, diskGroupId); else statement.BindNull(1);
    statement.BindInt64(2, diskId);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE && sqlite3_changes(connection_.Raw()) == 1;
}

bool Database::MoveDiskGroupToGroup(std::int64_t diskGroupId, std::int64_t parentGroupId) {
    if (!editable_ || diskGroupId == 0 || diskGroupId == parentGroupId) return false;
    if (!DiskGroupExists(connection_.Raw(), diskGroupId)) return false;
    if (parentGroupId != 0 && !DiskGroupExists(connection_.Raw(), parentGroupId)) return false;
    if (parentGroupId != 0) {
        SQLiteStatement cycleCheck(connection_.Raw(),
            "WITH RECURSIVE descendants(id) AS ("
            "SELECT id FROM disk_groups WHERE parent_group_id=? "
            "UNION ALL SELECT g.id FROM disk_groups g JOIN descendants d ON g.parent_group_id=d.id) "
            "SELECT EXISTS(SELECT 1 FROM descendants WHERE id=?);");
        cycleCheck.BindInt64(1, diskGroupId);
        cycleCheck.BindInt64(2, parentGroupId);
        if (sqlite3_step(cycleCheck.Raw()) != SQLITE_ROW || sqlite3_column_int(cycleCheck.Raw(), 0) != 0) {
            return false;
        }
    }
    SQLiteStatement statement(connection_.Raw(),
        "UPDATE disk_groups SET parent_group_id=?,updated_at=? WHERE id=?;");
    if (parentGroupId != 0) statement.BindInt64(1, parentGroupId); else statement.BindNull(1);
    statement.BindInt64(2, wit::platform::NowUnixSeconds());
    statement.BindInt64(3, diskGroupId);
    return sqlite3_step(statement.Raw()) == SQLITE_DONE && sqlite3_changes(connection_.Raw()) == 1;
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
    statement.BindInt64(9, file.modifiedAt);
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
    return browserRepository_.GetBrowserRootItemCount(location);
}

std::vector<wit::core::BrowserItem> Database::GetBrowserRootItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit) {
    return browserRepository_.GetBrowserRootItemsPage(location, offset, limit);
}

int Database::GetBrowserItemCount(const wit::core::BrowserLocation& location) {
    return browserRepository_.GetBrowserItemCount(location);
}

std::vector<wit::core::FileEntry> Database::GetBrowserItemsPage(
    const wit::core::BrowserLocation& location, int offset, int limit) {
    return browserRepository_.GetBrowserItemsPage(location, offset, limit);
}

bool Database::HasChildFolders(std::int64_t sourceId, const std::wstring& parentPath) {
    return browserRepository_.HasChildFolders(sourceId, parentPath);
}

std::vector<wit::core::FileEntry> Database::GetChildFolders(
    std::int64_t sourceId, const std::wstring& parentPath) {
    return browserRepository_.GetChildFolders(sourceId, parentPath);
}

int Database::GetItemSearchCount(const std::wstring& nameTerm) {
    return searchRepository_.CountByName(nameTerm);
}

std::vector<wit::core::FileEntry> Database::GetItemSearchPage(const std::wstring& nameTerm, int offset, int limit) {
    return searchRepository_.PageByName(nameTerm, offset, limit);
}
}
