#include "../../src/core/FileScanner.h"
#include "../../src/platform/Win32Helpers.h"
#include "../../src/storage/Database.h"
#include "../../third_party/sqlite/sqlite3.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int failures{};

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

sqlite3* OpenRaw(const std::filesystem::path& path) {
    sqlite3* db{};
    return sqlite3_open(wit::platform::ToUtf8(path.wstring()).c_str(), &db) == SQLITE_OK ? db : nullptr;
}

std::int64_t ScalarInt(const std::filesystem::path& path, const char* sql) {
    sqlite3* db = OpenRaw(path);
    sqlite3_stmt* statement{};
    std::int64_t value = -1;
    if (db && sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int64(statement, 0);
    }
    if (statement) sqlite3_finalize(statement);
    if (db) sqlite3_close(db);
    return value;
}

std::string ScalarText(const std::filesystem::path& path, const char* sql) {
    sqlite3* db = OpenRaw(path);
    sqlite3_stmt* statement{};
    std::string value;
    if (db && sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        if (text) value = text;
    }
    if (statement) sqlite3_finalize(statement);
    if (db) sqlite3_close(db);
    return value;
}

bool ExecRaw(const std::filesystem::path& path, const char* sql) {
    sqlite3* db = OpenRaw(path);
    const bool success = db && sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
    if (db) sqlite3_close(db);
    return success;
}
}

int wmain() {
    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-storage-smoke-" + std::to_wstring(GetCurrentProcessId()));
    const auto mediaWithCrc = testRoot / L"media-crc";
    const auto mediaWithoutCrc = testRoot / L"media-no-crc";
    const auto catalogPath = testRoot / L"catalog.db";
    const auto oldPath = testRoot / L"old.db";

    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(mediaWithCrc);
    std::filesystem::create_directories(mediaWithoutCrc);
    {
        std::ofstream(mediaWithCrc / L"example.txt", std::ios::binary) << "abc";
        std::ofstream(mediaWithoutCrc / L"plain", std::ios::binary) << "no crc";
    }
    SetFileAttributesW((mediaWithCrc / L"example.txt").c_str(), FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN);

    wit::storage::Database db;
    Check(db.CreateNew(catalogPath.wstring()), "fresh catalog creation");
    wit::storage::Database staged;
    Check(staged.CreateWorkingCopy(db), "working-copy creation");
    Check(staged.SetCatalogDescription(L"Smoke catalog"), "catalog description update");
    Check(staged.GetCatalogMetadata().description == L"Smoke catalog", "catalog description round-trip");

    wit::core::Disk first{};
    first.diskName = L"First";
    first.diskNumber = 1;
    first.sourcePath = mediaWithCrc.wstring();
    first.location = L"C:\\images\\stable-source.iso";
    first.totalCapacity = 1000;
    first.freeSpace = 250;
    first.addedAt = 100;
    first.updatedAt = 100;
    first.diskType = wit::core::DiskType::Other;
    first.id = staged.AddDisk(first);
    Check(first.id != 0, "first disk insert");
    Check(staged.FindDiskBySourcePath(L"Q:\\", first.location) == first.id,
        "stable original location identifies a remounted image");
    wit::core::FileScanner scanner;
    wit::core::FileScanner::Result firstResult{};
    Check(scanner.ScanFolder(first.sourcePath, first.id, staged, {}, firstResult, true), "CRC-enabled scan");
    first.totalFiles = firstResult.totalFiles;
    first.totalFolders = firstResult.totalFolders;
    first.updatedAt = 101;
    first.addedAt = 999;
    Check(staged.UpdateDisk(first), "first disk update");
    wit::core::DiskScanStatistics firstStats{first.id, 101, firstResult.elapsedMilliseconds, 0, true};
    Check(staged.UpdateDiskScanStatistics(firstStats), "first scan statistics");

    wit::core::Disk second{};
    second.diskName = L"Second";
    second.diskNumber = 2;
    second.sourcePath = mediaWithoutCrc.wstring();
    second.totalCapacity = 500;
    second.freeSpace = 700;
    second.addedAt = 200;
    second.updatedAt = 200;
    second.diskType = wit::core::DiskType::Other;
    second.id = staged.AddDisk(second);
    Check(second.id != 0, "second disk insert");
    wit::core::FileScanner::Result secondResult{};
    Check(scanner.ScanFolder(second.sourcePath, second.id, staged, {}, secondResult, false), "CRC-disabled scan");
    second.totalFiles = secondResult.totalFiles;
    second.totalFolders = secondResult.totalFolders;
    Check(staged.UpdateDisk(second), "second disk update");

    Check(db.GetCatalogSummary().totalDisks == 0 && db.GetCatalogMetadata().description.empty(),
        "staged edits do not mutate active catalog before save");
    Check(db.ReplaceCatalogDataFrom(staged), "staged save persistence");
    const auto summary = db.GetCatalogSummary();
    Check(summary.totalDisks == 2 && summary.totalFiles == 2 && summary.totalFolders == 2,
        "derived disk/file/folder totals");
    Check(summary.totalStorageSpace == 1500 && summary.totalUsedSpace == 750,
        "derived capacity totals with non-negative used space");
    Check(summary.catalogFileSize > 0, "catalog size is read from the filesystem");
    Check(db.GetBrowserItemCount({}) == 2, "normalized root browsing");
    wit::core::BrowserLocation firstLocation{};
    firstLocation.isRoot = false;
    firstLocation.sourceId = first.id;
    firstLocation.path = first.sourcePath;
    Check(db.GetBrowserItemsPage(firstLocation, 0, 10).size() == 1, "normalized disk browsing");
    Check(db.GetItemSearchCount(L"example") == 1 && db.GetItemSearchPage(L"example", 0, 10).size() == 1,
        "normalized search paging");
    wit::storage::Database discarded;
    Check(discarded.CreateWorkingCopy(db) && discarded.SetCatalogDescription(L"Discarded"),
        "discard candidate preparation");
    discarded.Close();
    Check(db.GetCatalogMetadata().description == L"Smoke catalog", "discard leaves active catalog unchanged");
    db.Close();

    Check(ScalarText(catalogPath, "SELECT extension FROM files WHERE name='example.txt';") == "txt",
        "extension excludes leading dot");
    Check(ScalarText(catalogPath, "SELECT crc FROM files WHERE name='example.txt';") == "352441C2",
        "enabled CRC-32 persistence");
    Check(ScalarInt(catalogPath, "SELECT COUNT(*) FROM files WHERE name='plain' AND crc IS NULL;") == 1,
        "disabled CRC remains null");
    Check((ScalarInt(catalogPath, "SELECT attributes FROM files WHERE name='example.txt';") &
        FILE_ATTRIBUTE_HIDDEN) != 0, "native file attribute persistence");
    Check(ScalarInt(catalogPath, "SELECT COUNT(*) FROM disks WHERE disk_name='First' AND added_at=100 AND updated_at=101;") == 1,
        "rescan update preserves added time and advances updated time");
    Check(ScalarInt(catalogPath, "SELECT COUNT(*) FROM pragma_table_info('catalog_metadata') "
        "WHERE name IN ('catalog_file_size','total_disks','total_files','total_folders','total_storage_space','total_used_space');") == 0,
        "catalog derived values are not stored");

    Check(ExecRaw(oldPath, "CREATE TABLE catalogs(id INTEGER PRIMARY KEY);"
        "CREATE TABLE files(id INTEGER PRIMARY KEY,is_directory INTEGER);"), "old-format fixture creation");
    wit::storage::Database old;
    Check(!old.OpenExisting(oldPath.wstring()), "old-format catalog rejection");
    Check(ScalarInt(oldPath, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='disks';") == 0,
        "old-format rejection does not add replacement tables");

    std::filesystem::remove_all(testRoot);
    if (failures != 0) return 1;
    std::cout << "Storage smoke check passed.\n";
    return 0;
}
