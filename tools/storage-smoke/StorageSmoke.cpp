#include "../../src/app/ScanCoordinator.h"
#include "../../src/core/FileScanner.h"
#include "../../src/platform/PathHelpers.h"
#include "../../src/platform/Win32Helpers.h"
#include "../../src/storage/Database.h"
#include "../../third_party/sqlite/sqlite3.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures{};
std::vector<wit::app::ScanId> completedScans;

LRESULT CALLBACK ScanTargetProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == wit::app::WM_SCAN_COMPLETE) {
        completedScans.push_back(static_cast<wit::app::ScanId>(wparam));
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

bool WaitForCompletion(wit::app::ScanId scanId) {
    const auto deadline = GetTickCount64() + 5000;
    while (GetTickCount64() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        for (const auto completed : completedScans) {
            if (completed == scanId) return true;
        }
        Sleep(1);
    }
    return false;
}

std::optional<wit::app::ScanResult> WaitForResult(wit::app::ScanCoordinator& coordinator, wit::app::ScanId scanId) {
    const auto deadline = GetTickCount64() + 5000;
    while (GetTickCount64() < deadline) {
        auto result = coordinator.TakeResult(scanId);
        if (result) return result;
        Sleep(1);
    }
    return std::nullopt;
}

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
    const auto normalizedWithoutSizePath = testRoot / L"normalized-without-size.db";

    Check(wit::platform::DisplayNameForPath(mediaWithCrc) == L"media-crc", "path display leaf name");
    Check(wit::platform::DisplayNameForPath(mediaWithCrc / L"") == L"media-crc",
        "path display ignores trailing separator");
    Check(wit::platform::DisplayNameForPath(testRoot.root_path()) == testRoot.root_path().wstring(),
        "path display retains drive root");

    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(mediaWithCrc);
    std::filesystem::create_directories(mediaWithCrc / L"container" / L"nested");
    std::filesystem::create_directories(mediaWithCrc / L"empty");
    std::filesystem::create_directories(mediaWithoutCrc);
    {
        std::ofstream(mediaWithCrc / L"example.txt", std::ios::binary) << "abc";
        std::ofstream(mediaWithCrc / L"container" / L"inside.bin", std::ios::binary) << "1234";
        std::ofstream(mediaWithCrc / L"container" / L"nested" / L"deeper.bin", std::ios::binary) << "12345";
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
    std::stop_source cancelledScan;
    cancelledScan.request_stop();
    wit::core::FileScanner::Result cancelledResult{};
    Check(!scanner.ScanFolder(first.sourcePath, first.id, staged, {}, cancelledResult, false, true,
        cancelledScan.get_token()), "pre-cancelled scan aborts cooperatively");
    wit::core::BrowserLocation cancelledLocation{};
    cancelledLocation.isRoot = false;
    cancelledLocation.sourceId = first.id;
    cancelledLocation.path = first.sourcePath;
    Check(staged.GetBrowserItemCount(cancelledLocation) == 0, "cancelled scan rolls back staged contents");
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
    Check(summary.totalDisks == 2 && summary.totalFiles == 4 && summary.totalFolders == 5,
        "derived disk/file/folder totals");
    Check(summary.totalStorageSpace == 1500 && summary.totalUsedSpace == 750,
        "derived capacity totals with non-negative used space");
    Check(summary.catalogFileSize > 0, "catalog size is read from the filesystem");
    Check(db.GetBrowserItemCount({}) == 2, "normalized root browsing");
    std::filesystem::remove_all(mediaWithCrc);
    wit::core::BrowserLocation firstLocation{};
    firstLocation.isRoot = false;
    firstLocation.sourceId = first.id;
    firstLocation.path = first.sourcePath;
    const auto firstItems = db.GetBrowserItemsPage(firstLocation, 0, 10);
    Check(firstItems.size() == 3, "normalized disk browsing remains available without source media");
    bool foundContainer{};
    bool foundEmpty{};
    for (const auto& item : firstItems) {
        if (item.name == L"container") foundContainer = item.isDirectory && item.size == 9;
        if (item.name == L"empty") foundEmpty = item.isDirectory && item.size == 0;
    }
    Check(foundContainer, "folder page exposes persisted recursive stored size");
    Check(foundEmpty, "folder page exposes persisted empty-folder size");
    Check(db.GetItemSearchCount(L"example") == 1 && db.GetItemSearchPage(L"example", 0, 10).size() == 1,
        "normalized search paging");
    wit::storage::Database discarded;
    Check(discarded.CreateWorkingCopy(db) && discarded.SetCatalogDescription(L"Discarded"),
        "discard candidate preparation");
    discarded.Close();
    Check(db.GetCatalogMetadata().description == L"Smoke catalog", "discard leaves active catalog unchanged");

    WNDCLASSW targetClass{};
    targetClass.lpfnWndProc = ScanTargetProc;
    targetClass.hInstance = GetModuleHandleW(nullptr);
    targetClass.lpszClassName = L"WhereIsThatScanSmokeTarget";
    RegisterClassW(&targetClass);
    const auto target = CreateWindowExW(0, targetClass.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
        nullptr, targetClass.hInstance, nullptr);
    Check(target != nullptr, "scan lifecycle test target creation");
    if (target) {
        wit::app::ScanCoordinator coordinator;
        Check(coordinator.AttachTarget(target), "scan lifecycle notification target attachment");
        wit::ui::AddNewDiskMediaResult media;
        media.kind = wit::ui::MediaSourceKind::Folder;
        media.originalPath = mediaWithoutCrc.wstring();
        media.scanRoot = media.originalPath;
        media.diskName = L"Coordinator";

        wit::app::ScanId completedId{};
        Check(coordinator.Start(&db, media, completedId), "coordinator completed scan start");
        Check(WaitForCompletion(completedId), "coordinator completed result notification");
        auto completed = coordinator.TakeResult(completedId);
        Check(completed && completed->outcome == wit::app::ScanOutcome::Completed && completed->pending,
            "coordinator completed result owns staged database");
        Check(!coordinator.TakeResult(completedId), "coordinator result consumption is exactly once");
        coordinator.RetireWorker(completedId);
        Check(!coordinator.TakeResult(0), "invalid completion id has no consumable result");
        Check(!coordinator.TakeResult(completedId + 1000), "unknown scan id has no consumable result");

        {
            std::ofstream slowFile(mediaWithoutCrc / L"cancel-check.bin", std::ios::binary);
            const std::string block(1024 * 1024, 'x');
            for (int blockIndex = 0; blockIndex < 16; ++blockIndex) slowFile.write(block.data(), block.size());
        }
        wit::app::ScanId cancelledId{};
        media.calculateCrc = true;
        Check(coordinator.Start(&db, media, cancelledId), "coordinator cancelled scan start");
        Check(coordinator.RequestCancel(), "coordinator cancellation request");
        Check(WaitForCompletion(cancelledId), "coordinator cancelled result notification");
        auto cancelled = coordinator.TakeResult(cancelledId);
        Check(cancelled && cancelled->outcome == wit::app::ScanOutcome::Cancelled && !cancelled->pending,
            "coordinator cancellation does not publish pending state");
        coordinator.RetireWorker(cancelledId);

        wit::app::ScanId failedId{};
        media.calculateCrc = false;
        media.scanRoot = (testRoot / L"missing-media").wstring();
        Check(coordinator.Start(&db, media, failedId), "coordinator failed scan start");
        Check(WaitForCompletion(failedId), "coordinator failed result notification");
        auto failed = coordinator.TakeResult(failedId);
        Check(failed && failed->outcome == wit::app::ScanOutcome::Failed && !failed->pending,
            "coordinator failure does not publish pending state");
        coordinator.RetireWorker(failedId);

        const auto deliveredBeforeDetach = completedScans.size();
        coordinator.DetachTarget();
        media.scanRoot = mediaWithoutCrc.wstring();
        wit::app::ScanId detachedId{};
        Check(coordinator.Start(&db, media, detachedId), "detached target scan start");
        auto detached = WaitForResult(coordinator, detachedId);
        Check(detached && detached->outcome == wit::app::ScanOutcome::Completed,
            "detached target result remains safely owned for reclamation");
        coordinator.RetireWorker(detachedId);
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        Check(completedScans.size() == deliveredBeforeDetach,
            "completion after target detach does not reach destroyed UI state");

        media.calculateCrc = true;
        for (int iteration = 0; iteration < 3; ++iteration) {
            Check(coordinator.AttachTarget(target), "rapid close target reattachment");
            wit::app::ScanId rapidCloseId{};
            Check(coordinator.Start(&db, media, rapidCloseId), "rapid close scan start");
            Check(coordinator.RequestCancel(), "rapid close cancellation request");
            coordinator.DetachTarget();
            auto rapidClose = WaitForResult(coordinator, rapidCloseId);
            Check(rapidClose && rapidClose->outcome == wit::app::ScanOutcome::Cancelled && !rapidClose->pending,
                "rapid start/close cancellation publishes no pending state");
            coordinator.RetireWorker(rapidCloseId);
        }
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        Check(completedScans.size() == deliveredBeforeDetach,
            "rapid close completions remain detached from UI state");
        DestroyWindow(target);
    }
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
    Check(ScalarInt(catalogPath, "SELECT content_size FROM folders WHERE name='media-crc';") == 12,
        "stored root folder size includes direct and nested files");
    Check(ScalarInt(catalogPath, "SELECT content_size FROM folders WHERE name='container';") == 9,
        "stored child folder size includes nested files");
    Check(ScalarInt(catalogPath, "SELECT content_size FROM folders WHERE name='nested';") == 5,
        "stored nested folder size is persisted");
    Check(ScalarInt(catalogPath, "SELECT content_size FROM folders WHERE name='empty';") == 0,
        "stored empty folder size is persisted");

    Check(ExecRaw(oldPath, "CREATE TABLE catalogs(id INTEGER PRIMARY KEY);"
        "CREATE TABLE files(id INTEGER PRIMARY KEY,is_directory INTEGER);"), "old-format fixture creation");
    wit::storage::Database old;
    Check(!old.OpenExisting(oldPath.wstring()), "old-format catalog rejection");
    Check(ScalarInt(oldPath, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='disks';") == 0,
        "old-format rejection does not add replacement tables");

    Check(ExecRaw(normalizedWithoutSizePath,
        "CREATE TABLE catalog_metadata(description TEXT);"
        "CREATE TABLE disks(disk_name TEXT,source_path TEXT,disk_type TEXT);"
        "CREATE TABLE disk_scan_statistics(last_scanned_at INTEGER);"
        "CREATE TABLE folders(parent_folder_id INTEGER,path TEXT);"
        "CREATE TABLE files(folder_id INTEGER,extension TEXT,crc TEXT,accessed_at INTEGER);"),
        "normalized-without-size fixture creation");
    wit::storage::Database normalizedWithoutSize;
    Check(!normalizedWithoutSize.OpenExisting(normalizedWithoutSizePath.wstring()),
        "normalized catalog without stored folder size rejection");
    Check(ScalarInt(normalizedWithoutSizePath,
        "SELECT COUNT(*) FROM pragma_table_info('folders') WHERE name='content_size';") == 0,
        "normalized catalog rejection does not backfill stored folder size");

    std::filesystem::remove_all(testRoot);
    if (failures != 0) return 1;
    std::cout << "Storage smoke check passed.\n";
    return 0;
}
