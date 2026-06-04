#include <gtest/gtest.h>
#include <wit_database/Database.h>
#include <wit_gui/CatalogWorkflowController.h>
#include <wit_gui/ScanCoordinator.h>
#include <wit_infra/PathHelpers.h>
#include <wit_infra/AppSettings.h>
#include <wit_infra/VolumeInfo.h>
#include <wit_infra/Win32Helpers.h>
#include <wit_scanner/FileScanner.h>
#include <wit_scanner/ScanRequest.h>
#include <sqlite3.h>
#include <Windows.h>
#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
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

class AppSettingsGuard {
public:
    AppSettingsGuard() : original_(wit::platform::LoadAppSettings()) {}
    ~AppSettingsGuard() {
        (void)wit::platform::SaveAppSettings(original_);
    }

    AppSettingsGuard(const AppSettingsGuard&) = delete;
    AppSettingsGuard& operator=(const AppSettingsGuard&) = delete;

private:
    wit::platform::AppSettings original_;
};

std::optional<wit::app::ScanResult> WaitForResult(wit::app::ScanCoordinator& coordinator, wit::app::ScanId scanId) {
    const auto deadline = GetTickCount64() + 5000;
    while (GetTickCount64() < deadline) {
        auto result = coordinator.TakeResult(scanId);
        if (result) return result;
        Sleep(1);
    }
    return std::nullopt;
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

struct ZipMember {
    const wchar_t* path;
    std::string contents;
};

bool WriteZip(const std::filesystem::path& path, const std::vector<ZipMember>& members) {
    auto* writer = archive_write_new();
    if (!writer || archive_write_set_format_zip(writer) != ARCHIVE_OK ||
        archive_write_open_filename_w(writer, path.c_str()) != ARCHIVE_OK) {
        if (writer) archive_write_free(writer);
        return false;
    }
    bool success = true;
    for (const auto& member : members) {
        auto* entry = archive_entry_new();
        archive_entry_copy_pathname_w(entry, member.path);
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_size(entry, static_cast<la_int64_t>(member.contents.size()));
        success = archive_write_header(writer, entry) == ARCHIVE_OK &&
            archive_write_data(writer, member.contents.data(), member.contents.size()) ==
                static_cast<la_ssize_t>(member.contents.size());
        archive_entry_free(entry);
        if (!success) break;
    }
    return archive_write_free(writer) == ARCHIVE_OK && success;
}
}

TEST(StorageSmoke, CatalogDatabaseScannerAndCoordinatorIntegration) {
    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-storage-smoke-" + std::to_wstring(GetCurrentProcessId()));
    const auto mediaWithCrc = testRoot / L"media-crc";
    const auto mediaWithoutCrc = testRoot / L"media-no-crc";
    const auto mediaArchives = testRoot / L"media-archives";
    const auto catalogPath = testRoot / L"catalog.db";
    const auto archiveCatalogPath = testRoot / L"archive-catalog.db";
    const auto oldPath = testRoot / L"old.db";
    const auto normalizedWithoutSizePath = testRoot / L"normalized-without-size.db";
    const auto normalizedWithoutArchivePath = testRoot / L"normalized-without-archive.db";

    EXPECT_TRUE(wit::platform::DisplayNameForPath(mediaWithCrc) == L"media-crc") << "path display leaf name";
    EXPECT_TRUE(wit::platform::DisplayNameForPath(mediaWithCrc / L"") == L"media-crc") << "path display ignores trailing separator";
    EXPECT_TRUE(wit::platform::DisplayNameForPath(testRoot.root_path()) == testRoot.root_path().wstring()) << "path display retains drive root";
    EXPECT_TRUE(wit::platform::ClassifyVolumeDiskType(wit::core::DiskType::Other, false, true, false, true) ==
        wit::core::DiskType::HardDisk) << "fixed seek-penalty media is classified as hard disk";
    EXPECT_TRUE(wit::platform::ClassifyVolumeDiskType(wit::core::DiskType::Other, false, true, false, false) ==
        wit::core::DiskType::SolidStateDisk) << "fixed no-seek-penalty media is classified as solid-state disk";
    EXPECT_TRUE(wit::platform::ClassifyVolumeDiskType(wit::core::DiskType::Other, false, true, false, std::nullopt) ==
        wit::core::DiskType::Other) << "fixed media with unavailable property retains unknown type";
    EXPECT_TRUE(wit::platform::ClassifyVolumeDiskType(wit::core::DiskType::VirtualDisk, true, true, true, true) ==
        wit::core::DiskType::VirtualDisk) << "explicit virtual media classification is retained";
    EXPECT_TRUE(wit::platform::ClassifyVolumeDiskType(wit::core::DiskType::Other, true, false, false, std::nullopt) ==
        wit::core::DiskType::RemovableUSB) << "removable media classification is retained";
    EXPECT_TRUE(wit::platform::ClassifyVolumeDiskType(wit::core::DiskType::Other, false, false, true, std::nullopt) ==
        wit::core::DiskType::CD) << "optical media is classified as CD/DVD media";

    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(mediaWithCrc);
    std::filesystem::create_directories(mediaWithCrc / L"container" / L"nested");
    std::filesystem::create_directories(mediaWithCrc / L"empty");
    std::filesystem::create_directories(mediaWithoutCrc);
    std::filesystem::create_directories(mediaArchives);
    {
        std::ofstream(mediaWithCrc / L"example.txt", std::ios::binary) << "abc";
        std::ofstream(mediaWithCrc / L"container" / L"inside.bin", std::ios::binary) << "1234";
        std::ofstream(mediaWithCrc / L"container" / L"nested" / L"deeper.bin", std::ios::binary) << "12345";
        std::ofstream(mediaWithoutCrc / L"plain", std::ios::binary) << "no crc";
        std::ofstream(mediaArchives / L"ordinary.txt", std::ios::binary) << "plain";
        std::ofstream(mediaArchives / L"broken.zip", std::ios::binary) << "not zip";
    }
    EXPECT_TRUE(WriteZip(mediaArchives / L"readable.zip", {{L"nested/inner.txt", "abcde"}, {L"top.bin", "12"}})) << "readable archive fixture creation";
    EXPECT_TRUE(WriteZip(mediaArchives / L"empty.zip", {})) << "empty archive fixture creation";
    EXPECT_TRUE(WriteZip(mediaArchives / L"unsafe.zip", {{L"../escape.txt", "bad"}})) << "unsafe archive fixture creation";
    SetFileAttributesW((mediaWithCrc / L"example.txt").c_str(), FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN);

    wit::storage::Database db;
    EXPECT_TRUE(db.CreateNew(catalogPath.wstring(), false)) << "fresh catalog creation";
    wit::storage::Database staged;
    EXPECT_TRUE(staged.CreateWorkingCopy(db)) << "working-copy creation";
    EXPECT_TRUE(staged.SetCatalogDescription(L"Smoke catalog")) << "catalog description update";
    EXPECT_TRUE(staged.GetCatalogMetadata().description == L"Smoke catalog") << "catalog description round-trip";
    const auto smokeGroupId = staged.CreateDiskGroup(L"Smoke Group");
    EXPECT_TRUE(smokeGroupId != 0) << "disk group creation";

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
    EXPECT_TRUE(first.id != 0) << "first disk insert";
    EXPECT_TRUE(staged.FindDiskBySourcePath(L"Q:\\", first.location) == first.id) << "stable original location identifies a remounted image";
    wit::core::FileScanner scanner;
    std::stop_source cancelledScan;
    cancelledScan.request_stop();
    wit::core::FileScanner::Result cancelledResult{};
    EXPECT_TRUE(!scanner.ScanFolder(first.sourcePath, first.id, staged, {}, cancelledResult, false, true,
        cancelledScan.get_token())) << "pre-cancelled scan aborts cooperatively";
    wit::core::BrowserLocation cancelledLocation{};
    cancelledLocation.isRoot = false;
    cancelledLocation.sourceId = first.id;
    cancelledLocation.path = first.sourcePath;
    EXPECT_TRUE(staged.GetBrowserItemCount(cancelledLocation) == 0) << "cancelled scan rolls back staged contents";
    wit::core::FileScanner::Result firstResult{};
    EXPECT_TRUE(scanner.ScanFolder(first.sourcePath, first.id, staged, {}, firstResult, true)) << "CRC-enabled scan";
    first.totalFiles = firstResult.totalFiles;
    first.totalFolders = firstResult.totalFolders;
    first.updatedAt = 101;
    first.addedAt = 999;
    EXPECT_TRUE(staged.UpdateDisk(first)) << "first disk update";
    wit::core::DiskScanStatistics firstStats{first.id, 101, firstResult.elapsedMilliseconds, 0, true};
    EXPECT_TRUE(staged.UpdateDiskScanStatistics(firstStats)) << "first scan statistics";

    wit::core::Disk second{};
    second.diskGroupId = smokeGroupId;
    second.diskName = L"Second";
    second.diskNumber = 2;
    second.sourcePath = mediaWithoutCrc.wstring();
    second.totalCapacity = 500;
    second.freeSpace = 700;
    second.addedAt = 200;
    second.updatedAt = 200;
    second.diskType = wit::core::DiskType::Other;
    second.id = staged.AddDisk(second);
    EXPECT_TRUE(second.id != 0) << "second disk insert";
    wit::core::FileScanner::Result secondResult{};
    EXPECT_TRUE(scanner.ScanFolder(second.sourcePath, second.id, staged, {}, secondResult, false)) << "CRC-disabled scan";
    second.totalFiles = secondResult.totalFiles;
    second.totalFolders = secondResult.totalFolders;
    EXPECT_TRUE(staged.UpdateDisk(second)) << "second disk update";

    EXPECT_TRUE(db.GetCatalogSummary().totalDisks == 0 && db.GetCatalogMetadata().description.empty()) << "staged edits do not mutate active catalog before save";
    EXPECT_TRUE(db.SaveCatalogDataFrom(staged)) << "staged save persistence";
    const auto summary = db.GetCatalogSummary();
    EXPECT_TRUE(summary.totalDisks == 2 && summary.totalFiles == 4 && summary.totalFolders == 5) << "derived disk/file/folder totals";
    EXPECT_TRUE(summary.totalStorageSpace == 1500 && summary.totalUsedSpace == 750) << "derived capacity totals with non-negative used space";
    EXPECT_TRUE(summary.catalogFileSize > 0) << "catalog size is read from the filesystem";
    EXPECT_TRUE(db.GetBrowserItemCount({}) == 2) << "normalized root browsing";
    wit::core::BrowserLocation groupLocation{};
    groupLocation.isRoot = false;
    groupLocation.isDiskGroup = true;
    groupLocation.diskGroupId = smokeGroupId;
    EXPECT_TRUE(db.GetBrowserRootItemCount(groupLocation) == 1) << "disk group browser count";
    EXPECT_TRUE(db.GetBrowserRootItemsPage(groupLocation, 0, 10).size() == 1) << "disk group browser page";
    bool foundSmokeGroupSize{};
    for (const auto& item : db.GetBrowserRootItemsPage({}, 0, 10)) {
        if (item.type == wit::core::BrowserItemType::DiskGroup && item.group.id == smokeGroupId) {
            foundSmokeGroupSize = item.group.totalCapacity == 500 && item.group.freeSpace == 700 &&
                item.group.totalDisks == 1;
        }
    }
    EXPECT_TRUE(foundSmokeGroupSize) << "disk group browser row exposes aggregate storage size";
    wit::storage::Database hierarchy;
    EXPECT_TRUE(hierarchy.CreateWorkingCopy(db)) << "hierarchy working-copy creation";
    EXPECT_TRUE(hierarchy.MoveDiskToGroup(second.id, 0)) << "disk move to catalog root";
    EXPECT_TRUE(hierarchy.GetBrowserRootItemCount({}) == 3) << "disk move to root updates browser root count";
    EXPECT_TRUE(hierarchy.GetBrowserRootItemCount(groupLocation) == 0) << "disk move to root updates source group count";
    const auto childGroupId = hierarchy.CreateDiskGroup(L"Child Group");
    EXPECT_TRUE(childGroupId != 0) << "child disk group creation";
    EXPECT_TRUE(hierarchy.MoveDiskGroupToGroup(childGroupId, smokeGroupId)) << "disk group move to another group";
    EXPECT_TRUE(hierarchy.GetBrowserRootItemCount({}) == 3) << "nested group leaves catalog root";
    EXPECT_TRUE(hierarchy.GetBrowserRootItemCount(groupLocation) == 1) << "parent group exposes child group";
    EXPECT_TRUE(hierarchy.MoveDiskToGroup(second.id, childGroupId)) << "disk move to nested group";
    bool foundRecursiveGroupSize{};
    for (const auto& item : hierarchy.GetBrowserRootItemsPage({}, 0, 10)) {
        if (item.type == wit::core::BrowserItemType::DiskGroup && item.group.id == smokeGroupId) {
            foundRecursiveGroupSize = item.group.totalCapacity == 500 && item.group.freeSpace == 700 &&
                item.group.totalDisks == 1;
        }
    }
    EXPECT_TRUE(foundRecursiveGroupSize) << "parent group browser row includes nested group storage size";
    EXPECT_TRUE(!hierarchy.MoveDiskGroupToGroup(smokeGroupId, childGroupId)) << "group move rejects descendant-parent";
    EXPECT_TRUE(hierarchy.MoveDiskGroupToGroup(childGroupId, 0)) << "disk group move to catalog root";
    EXPECT_TRUE(hierarchy.GetBrowserRootItemCount({}) == 3) << "group move to root updates browser root count";
    EXPECT_TRUE(!hierarchy.MoveDiskGroupToGroup(smokeGroupId, smokeGroupId)) << "group move rejects self-parent";
    std::filesystem::remove_all(mediaWithCrc);
    wit::core::BrowserLocation firstLocation{};
    firstLocation.isRoot = false;
    firstLocation.sourceId = first.id;
    firstLocation.path = first.sourcePath;
    const auto firstItems = db.GetBrowserItemsPage(firstLocation, 0, 10);
    EXPECT_TRUE(firstItems.size() == 3) << "normalized disk browsing remains available without source media";
    bool foundContainer{};
    bool foundEmpty{};
    for (const auto& item : firstItems) {
        if (item.name == L"container") foundContainer = item.isDirectory && item.size == 9;
        if (item.name == L"empty") foundEmpty = item.isDirectory && item.size == 0;
    }
    EXPECT_TRUE(foundContainer) << "folder page exposes persisted recursive stored size";
    EXPECT_TRUE(foundEmpty) << "folder page exposes persisted empty-folder size";
    EXPECT_TRUE(db.GetItemSearchCount(L"example") == 1 && db.GetItemSearchPage(L"example", 0, 10).size() == 1) << "normalized search paging";
    wit::storage::Database discarded;
    EXPECT_TRUE(discarded.CreateWorkingCopy(db) && discarded.SetCatalogDescription(L"Discarded")) << "discard candidate preparation";
    discarded.Close();
    EXPECT_TRUE(db.GetCatalogMetadata().description == L"Smoke catalog") << "discard leaves active catalog unchanged";

    WNDCLASSW targetClass{};
    targetClass.lpfnWndProc = ScanTargetProc;
    targetClass.hInstance = GetModuleHandleW(nullptr);
    targetClass.lpszClassName = L"WhereIsThatScanSmokeTarget";
    RegisterClassW(&targetClass);
    const auto target = CreateWindowExW(0, targetClass.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
        nullptr, targetClass.hInstance, nullptr);
    EXPECT_TRUE(target != nullptr) << "scan lifecycle test target creation";
    if (target) {
        wit::app::ScanCoordinator coordinator;
        EXPECT_TRUE(coordinator.AttachTarget(target)) << "scan lifecycle notification target attachment";
        wit::core::ScanRequest media;
        media.kind = wit::core::MediaSourceKind::Folder;
        media.originalPath = mediaWithoutCrc.wstring();
        media.scanRoot = media.originalPath;
        media.diskName = L"Coordinator";

        wit::app::ScanId completedId{};
        EXPECT_TRUE(coordinator.Start(&db, media, completedId)) << "coordinator completed scan start";
        EXPECT_TRUE(WaitForCompletion(completedId)) << "coordinator completed result notification";
        auto completed = coordinator.TakeResult(completedId);
        EXPECT_TRUE(completed && completed->outcome == wit::app::ScanOutcome::Completed && completed->pending) << "coordinator completed result owns staged database";
        EXPECT_TRUE(!coordinator.TakeResult(completedId)) << "coordinator result consumption is exactly once";
        coordinator.RetireWorker(completedId);
        EXPECT_TRUE(!coordinator.TakeResult(0)) << "invalid completion id has no consumable result";
        EXPECT_TRUE(!coordinator.TakeResult(completedId + 1000)) << "unknown scan id has no consumable result";

        {
            std::ofstream slowFile(mediaWithoutCrc / L"cancel-check.bin", std::ios::binary);
            const std::string block(1024 * 1024, 'x');
            for (int blockIndex = 0; blockIndex < 16; ++blockIndex) slowFile.write(block.data(), block.size());
        }
        wit::app::ScanId cancelledId{};
        media.calculateCrc = true;
        EXPECT_TRUE(coordinator.Start(&db, media, cancelledId)) << "coordinator cancelled scan start";
        EXPECT_TRUE(coordinator.RequestCancel()) << "coordinator cancellation request";
        EXPECT_TRUE(WaitForCompletion(cancelledId)) << "coordinator cancelled result notification";
        auto cancelled = coordinator.TakeResult(cancelledId);
        EXPECT_TRUE(cancelled && cancelled->outcome == wit::app::ScanOutcome::Cancelled && !cancelled->pending) << "coordinator cancellation does not publish pending state";
        coordinator.RetireWorker(cancelledId);

        wit::app::ScanId failedId{};
        media.calculateCrc = false;
        media.scanRoot = wit::platform::Join(testRoot.wstring(), L"missing-media");
        EXPECT_TRUE(coordinator.Start(&db, media, failedId)) << "coordinator failed scan start";
        EXPECT_TRUE(WaitForCompletion(failedId)) << "coordinator failed result notification";
        auto failed = coordinator.TakeResult(failedId);
        EXPECT_TRUE(failed && failed->outcome == wit::app::ScanOutcome::Failed && !failed->pending) << "coordinator failure does not publish pending state";
        coordinator.RetireWorker(failedId);

        const auto deliveredBeforeDetach = completedScans.size();
        coordinator.DetachTarget();
        media.scanRoot = mediaWithoutCrc.wstring();
        wit::app::ScanId detachedId{};
        EXPECT_TRUE(coordinator.Start(&db, media, detachedId)) << "detached target scan start";
        auto detached = WaitForResult(coordinator, detachedId);
        EXPECT_TRUE(detached && detached->outcome == wit::app::ScanOutcome::Completed) << "detached target result remains safely owned for reclamation";
        coordinator.RetireWorker(detachedId);
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        EXPECT_TRUE(completedScans.size() == deliveredBeforeDetach) << "completion after target detach does not reach destroyed UI state";

        media.calculateCrc = true;
        for (int iteration = 0; iteration < 3; ++iteration) {
            EXPECT_TRUE(coordinator.AttachTarget(target)) << "rapid close target reattachment";
            wit::app::ScanId rapidCloseId{};
            EXPECT_TRUE(coordinator.Start(&db, media, rapidCloseId)) << "rapid close scan start";
            EXPECT_TRUE(coordinator.RequestCancel()) << "rapid close cancellation request";
            coordinator.DetachTarget();
            auto rapidClose = WaitForResult(coordinator, rapidCloseId);
            EXPECT_TRUE(rapidClose && rapidClose->outcome == wit::app::ScanOutcome::Cancelled && !rapidClose->pending) << "rapid start/close cancellation publishes no pending state";
            coordinator.RetireWorker(rapidCloseId);
        }
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        EXPECT_TRUE(completedScans.size() == deliveredBeforeDetach) << "rapid close completions remain detached from UI state";
        DestroyWindow(target);
    }
    db.Close();

    EXPECT_TRUE(ScalarText(catalogPath, "SELECT extension FROM files WHERE name='example.txt';") == "txt") << "extension excludes leading dot";
    EXPECT_TRUE(ScalarText(catalogPath, "SELECT crc FROM files WHERE name='example.txt';") == "352441C2") << "enabled CRC-32 persistence";
    EXPECT_TRUE(ScalarInt(catalogPath, "SELECT COUNT(*) FROM files WHERE name='plain' AND crc IS NULL;") == 1) << "disabled CRC remains null";
    EXPECT_TRUE((ScalarInt(catalogPath, "SELECT attributes FROM files WHERE name='example.txt';") &
        FILE_ATTRIBUTE_HIDDEN) != 0) << "native file attribute persistence";
    EXPECT_TRUE(ScalarInt(catalogPath, "SELECT COUNT(*) FROM disks WHERE disk_name='First' AND added_at=100 AND updated_at=101;") == 1) << "rescan update preserves added time and advances updated time";
    EXPECT_TRUE(ScalarInt(catalogPath, "SELECT COUNT(*) FROM pragma_table_info('catalog_metadata') "
        "WHERE name IN ('catalog_file_size','total_disks','total_files','total_folders','total_storage_space','total_used_space');") == 0)
        << "catalog derived values are not stored";
    EXPECT_TRUE(ScalarInt(catalogPath, "SELECT content_size FROM folders WHERE name='media-crc';") == 12) << "stored root folder size includes direct and nested files";
    EXPECT_TRUE(ScalarInt(catalogPath, "SELECT content_size FROM folders WHERE name='container';") == 9) << "stored child folder size includes nested files";
    EXPECT_TRUE(ScalarInt(catalogPath, "SELECT content_size FROM folders WHERE name='nested';") == 5) << "stored nested folder size is persisted";
    EXPECT_TRUE(ScalarInt(catalogPath, "SELECT content_size FROM folders WHERE name='empty';") == 0) << "stored empty folder size is persisted";

    wit::storage::Database archiveDb;
    EXPECT_TRUE(archiveDb.CreateNew(archiveCatalogPath.wstring(), false)) << "archive catalog creation";
    wit::core::Disk archiveDisk{};
    archiveDisk.diskName = L"Archives";
    archiveDisk.sourcePath = mediaArchives.wstring();
    archiveDisk.addedAt = 300;
    archiveDisk.updatedAt = 300;
    archiveDisk.id = archiveDb.AddDisk(archiveDisk);
    EXPECT_TRUE(archiveDisk.id != 0) << "archive disk insert";
    std::uint64_t physicalArchiveFiles{};
    EXPECT_TRUE(scanner.CountFiles(archiveDisk.sourcePath, physicalArchiveFiles) && physicalArchiveFiles == 5) << "progress pre-count treats each archive as one physical file";
    std::stop_source cancelledArchiveScan;
    cancelledArchiveScan.request_stop();
    wit::core::FileScanner::Result cancelledArchiveResult{};
    EXPECT_TRUE(!scanner.ScanFolder(archiveDisk.sourcePath, archiveDisk.id, archiveDb, {}, cancelledArchiveResult,
        false, true, cancelledArchiveScan.get_token(), true)) << "archive-enabled cancelled scan aborts";
    wit::core::BrowserLocation cancelledArchiveLocation{};
    cancelledArchiveLocation.isRoot = false;
    cancelledArchiveLocation.sourceId = archiveDisk.id;
    cancelledArchiveLocation.path = archiveDisk.sourcePath;
    EXPECT_TRUE(archiveDb.GetBrowserItemCount(cancelledArchiveLocation) == 0) << "archive-enabled cancelled scan publishes no virtual content";
    std::vector<std::wstring> diagnostics;
    wit::core::FileScanner::Progress finalArchiveProgress{};
    wit::core::FileScanner::Result archiveResult{};
    EXPECT_TRUE(scanner.ScanFolder(archiveDisk.sourcePath, archiveDisk.id, archiveDb,
        [&finalArchiveProgress](const wit::core::FileScanner::Progress& progress) {
            finalArchiveProgress = progress;
        }, archiveResult, true, true, {}, true,
        [&diagnostics](const std::wstring& message) { diagnostics.push_back(message); }))
        << "archive-enabled scan";
    EXPECT_TRUE(archiveResult.scannedArchives == 2 && archiveResult.archiveFiles == 2 &&
        archiveResult.archiveFolders == 1) << "archive result counts readable containers and member hierarchy";
    EXPECT_TRUE(archiveResult.totalFiles == 5 && archiveResult.totalFolders == 4) << "archive representation replaces readable container files in ordinary totals";
    EXPECT_TRUE(finalArchiveProgress.scannedFiles == physicalArchiveFiles) << "archive scan progress decrements once per physical file";
    EXPECT_TRUE(diagnostics.size() >= 2) << "unreadable and unsafe archives produce nonfatal diagnostics";
    wit::core::DiskScanStatistics archiveStats{archiveDisk.id, 301, archiveResult.elapsedMilliseconds, 0, true,
        static_cast<std::int64_t>(archiveResult.scannedArchives),
        static_cast<std::int64_t>(archiveResult.archiveFiles),
        static_cast<std::int64_t>(archiveResult.archiveFolders)};
    EXPECT_TRUE(archiveDb.UpdateDiskScanStatistics(archiveStats)) << "archive scan statistics persistence";
    EXPECT_TRUE(ScalarText(archiveCatalogPath, "SELECT entry_type FROM folders WHERE name='readable.zip';") == "archive") << "readable archive is represented as archive-backed folder";
    EXPECT_TRUE(ScalarInt(archiveCatalogPath, "SELECT COUNT(*) FROM files WHERE name='readable.zip';") == 0) << "readable archive is not duplicated as ordinary file";
    EXPECT_TRUE(ScalarText(archiveCatalogPath, "SELECT entry_type FROM folders WHERE name='empty.zip';") == "archive" &&
        ScalarInt(archiveCatalogPath, "SELECT content_size FROM folders WHERE name='empty.zip';") == 0)
        << "empty readable archive remains navigable";
    EXPECT_TRUE(ScalarInt(archiveCatalogPath, "SELECT COUNT(*) FROM folders WHERE name='nested' AND entry_type='directory';") == 1 &&
        ScalarInt(archiveCatalogPath, "SELECT COUNT(*) FROM files WHERE name='inner.txt';") == 1)
        << "omitted archive parent directories are synthesized";
    EXPECT_TRUE(ScalarInt(archiveCatalogPath, "SELECT content_size FROM folders WHERE name='readable.zip';") == 7 &&
        ScalarInt(archiveCatalogPath, "SELECT content_size FROM folders WHERE name='nested';") == 5)
        << "archive content sizes use stored uncompressed member sizes";
    EXPECT_TRUE(ScalarInt(archiveCatalogPath, "SELECT COUNT(*) FROM files WHERE name='inner.txt' AND crc IS NOT NULL;") == 1) << "archive member CRC is calculated when enabled";
    EXPECT_TRUE(ScalarInt(archiveCatalogPath, "SELECT COUNT(*) FROM files WHERE name IN ('broken.zip','unsafe.zip');") == 2 &&
        ScalarInt(archiveCatalogPath, "SELECT COUNT(*) FROM folders WHERE name IN ('broken.zip','unsafe.zip');") == 0)
        << "failed archives fall back to files without virtual rows";
    EXPECT_TRUE(ScalarInt(archiveCatalogPath, "SELECT scanned_archives FROM disk_scan_statistics;") == 2 &&
        ScalarInt(archiveCatalogPath, "SELECT archive_files_count FROM disk_scan_statistics;") == 2 &&
        ScalarInt(archiveCatalogPath, "SELECT archive_folders_count FROM disk_scan_statistics;") == 1)
        << "latest statistics persist archive counts";
    wit::core::BrowserLocation archiveLocation{};
    archiveLocation.isRoot = false;
    archiveLocation.sourceId = archiveDisk.id;
    archiveLocation.path = archiveDisk.sourcePath;
    const auto archiveItems = archiveDb.GetBrowserItemsPage(archiveLocation, 0, 20);
    bool foundReadableArchive{};
    for (const auto& item : archiveItems) {
        if (item.name == L"readable.zip") foundReadableArchive = item.isDirectory && item.isArchive;
    }
    EXPECT_TRUE(foundReadableArchive) << "browser projection distinguishes navigable archive folders";
    archiveLocation.path = wit::platform::Join(mediaArchives.wstring(), L"readable.zip");
    const auto readableArchiveItems = archiveDb.GetBrowserItemsPage(archiveLocation, 0, 20);
    EXPECT_TRUE(readableArchiveItems.size() == 2) << "archive folder uses ordinary immediate-content navigation";

    wit::core::Disk disabledArchiveDisk{};
    disabledArchiveDisk.diskName = L"Archives disabled";
    disabledArchiveDisk.sourcePath = mediaArchives.wstring() + L"-disabled";
    std::filesystem::create_directories(disabledArchiveDisk.sourcePath);
    EXPECT_TRUE(WriteZip(wit::platform::Join(disabledArchiveDisk.sourcePath, L"readable.zip"),
        {{L"nested/inner.txt", "abcde"}})) << "disabled archive fixture creation";
    disabledArchiveDisk.addedAt = 302;
    disabledArchiveDisk.updatedAt = 302;
    disabledArchiveDisk.id = archiveDb.AddDisk(disabledArchiveDisk);
    wit::core::FileScanner::Result disabledResult{};
    EXPECT_TRUE(disabledArchiveDisk.id != 0 && scanner.ScanFolder(disabledArchiveDisk.sourcePath, disabledArchiveDisk.id,
        archiveDb, {}, disabledResult, false)) << "archive-disabled scan";
    EXPECT_TRUE(disabledResult.scannedArchives == 0 && disabledResult.archiveFiles == 0 &&
        ScalarInt(archiveCatalogPath, "SELECT COUNT(*) FROM folders WHERE disk_id=2 AND entry_type='archive';") == 0)
        << "archive-disabled scan keeps archive as ordinary file";
    archiveDb.Close();

    EXPECT_TRUE(ExecRaw(oldPath, "CREATE TABLE catalogs(id INTEGER PRIMARY KEY);"
        "CREATE TABLE files(id INTEGER PRIMARY KEY,is_directory INTEGER);")) << "old-format fixture creation";
    wit::storage::Database old;
    EXPECT_TRUE(!old.OpenExisting(oldPath.wstring())) << "old-format catalog rejection";
    EXPECT_TRUE(ScalarInt(oldPath, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='disks';") == 0) << "old-format rejection does not add replacement tables";

    EXPECT_TRUE(ExecRaw(normalizedWithoutSizePath,
        "CREATE TABLE catalog_metadata(description TEXT);"
        "CREATE TABLE disks(disk_name TEXT,source_path TEXT,disk_type TEXT);"
        "CREATE TABLE disk_scan_statistics(last_scanned_at INTEGER);"
        "CREATE TABLE folders(parent_folder_id INTEGER,path TEXT);"
        "CREATE TABLE files(folder_id INTEGER,extension TEXT,crc TEXT,accessed_at INTEGER);"))
        << "normalized-without-size fixture creation";
    wit::storage::Database normalizedWithoutSize;
    EXPECT_TRUE(!normalizedWithoutSize.OpenExisting(normalizedWithoutSizePath.wstring())) << "normalized catalog without stored folder size rejection";
    EXPECT_TRUE(ScalarInt(normalizedWithoutSizePath,
        "SELECT COUNT(*) FROM pragma_table_info('folders') WHERE name='content_size';") == 0)
        << "normalized catalog rejection does not backfill stored folder size";

    EXPECT_TRUE(ExecRaw(normalizedWithoutArchivePath,
        "CREATE TABLE catalog_metadata(id INTEGER PRIMARY KEY,description TEXT);"
        "CREATE TABLE disks(id INTEGER PRIMARY KEY,disk_name TEXT,source_path TEXT,disk_type TEXT);"
        "CREATE TABLE disk_scan_statistics(disk_id INTEGER PRIMARY KEY,last_scanned_at INTEGER,"
        "image_scanning_time_ms INTEGER,imported_descriptions_count INTEGER,calculated_file_crcs INTEGER);"
        "CREATE TABLE folders(id INTEGER PRIMARY KEY,disk_id INTEGER,parent_folder_id INTEGER,path TEXT,"
        "content_size INTEGER);"
        "CREATE TABLE files(folder_id INTEGER,extension TEXT,crc TEXT,accessed_at INTEGER);"))
        << "normalized-without-archive fixture creation";
    wit::storage::Database normalizedWithoutArchive;
    EXPECT_TRUE(!normalizedWithoutArchive.OpenExisting(normalizedWithoutArchivePath.wstring())) << "normalized catalog without archive-aware fields rejection";
    EXPECT_TRUE(ScalarInt(normalizedWithoutArchivePath,
        "SELECT COUNT(*) FROM pragma_table_info('folders') WHERE name='entry_type';") == 0)
        << "normalized archive-field rejection does not backfill schema";

    std::filesystem::remove_all(testRoot);
}

TEST(StorageSmoke, ImportedCatalogGroupAndDiskMovesSaveThroughAppWorkflow) {
    const auto sourceCatalogPath = std::filesystem::current_path() / L"tests" / L"d-import-test.db";
    if (!std::filesystem::exists(sourceCatalogPath)) {
        GTEST_SKIP() << "tests\\d-import-test.db fixture is not available";
    }

    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-imported-catalog-save-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);
    const auto catalogPath = testRoot / L"d-import-test-copy.db";
    std::filesystem::copy_file(sourceCatalogPath, catalogPath, std::filesystem::copy_options::overwrite_existing);

    {
        wit::app::CatalogWorkflowController controller;
        auto openResult = controller.OpenCatalogPathSelected(catalogPath.wstring());
        ASSERT_TRUE(openResult.messages.empty()) << "catalog opens through app controller";
        ASSERT_FALSE(openResult.browserEffects.empty()) << "open publishes browser effect";
        const auto catalogId = openResult.browserEffects.front().catalogId;
        ASSERT_NE(catalogId, 0);

        auto* database = controller.WorkingDatabase(catalogId);
        ASSERT_NE(database, nullptr);
        ASSERT_TRUE(database->IsEditable());

        const auto groups = database->GetDiskGroups();
        const auto disks = database->GetDisksPage(0, 20);
        ASSERT_GE(groups.size(), 20u);
        ASSERT_GE(disks.size(), 10u);

        std::int64_t firstRootGroupId{};
        std::int64_t secondRootGroupId{};
        for (const auto& group : groups) {
            if (group.parentGroupId != 0) continue;
            int childCount{};
            for (const auto& candidate : groups) {
                if (candidate.parentGroupId == group.id) ++childCount;
            }
            if (childCount >= 10 && firstRootGroupId == 0) {
                firstRootGroupId = group.id;
                continue;
            }
            if (secondRootGroupId == 0) {
                secondRootGroupId = group.id;
            }
        }
        ASSERT_NE(firstRootGroupId, 0);
        ASSERT_NE(secondRootGroupId, 0);

        std::vector<std::int64_t> movedGroupIds;
        for (const auto& group : groups) {
            if (group.parentGroupId == firstRootGroupId) {
                movedGroupIds.push_back(group.id);
                auto moveResult = controller.MoveDiskGroupToGroup(catalogId, group.id, secondRootGroupId);
                ASSERT_TRUE(moveResult.messages.empty()) << "group move is staged";
                if (movedGroupIds.size() == 10) break;
            }
        }
        ASSERT_EQ(movedGroupIds.size(), 10u);

        std::vector<std::int64_t> movedDiskIds;
        for (std::size_t index = 0; index < 10; ++index) {
            const auto targetGroupId = movedGroupIds[index % movedGroupIds.size()];
            auto moveResult = controller.MoveDiskToGroup(catalogId, disks[index].id, targetGroupId);
            ASSERT_TRUE(moveResult.messages.empty()) << "disk move is staged";
            movedDiskIds.push_back(disks[index].id);
        }

        auto saveResult = controller.RequestSave();
        ASSERT_TRUE(saveResult.messages.empty()) << "pending moves save through app workflow";
        ASSERT_TRUE(saveResult.presentation.catalogStatus == L"Loaded") << "catalog is clean after save";

        wit::storage::Database reopened;
        ASSERT_TRUE(reopened.OpenExisting(catalogPath.wstring())) << "saved imported catalog reopens";
        const auto reopenedGroups = reopened.GetDiskGroups();
        for (const auto movedGroupId : movedGroupIds) {
            const auto found = std::find_if(reopenedGroups.begin(), reopenedGroups.end(),
                [movedGroupId](const wit::core::DiskGroup& group) { return group.id == movedGroupId; });
            ASSERT_NE(found, reopenedGroups.end());
            EXPECT_EQ(found->parentGroupId, secondRootGroupId) << "moved group parent persisted";
        }

        const auto reopenedDisks = reopened.GetDisksPage(0, 20);
        for (std::size_t index = 0; index < movedDiskIds.size(); ++index) {
            const auto movedDiskId = movedDiskIds[index];
            const auto expectedGroupId = movedGroupIds[index % movedGroupIds.size()];
            const auto found = std::find_if(reopenedDisks.begin(), reopenedDisks.end(),
                [movedDiskId](const wit::core::Disk& disk) { return disk.id == movedDiskId; });
            ASSERT_NE(found, reopenedDisks.end());
            EXPECT_EQ(found->diskGroupId, expectedGroupId) << "moved disk group persisted";
        }
        reopened.Close();
    }
    std::filesystem::remove_all(testRoot);
}

TEST(StorageSmoke, ClosingLastCatalogClearsStartupRestorePath) {
    AppSettingsGuard settingsGuard;

    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-close-catalog-settings-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);
    const auto catalogPath = testRoot / L"startup-restore.db";
    const auto normalizedCatalogPath = std::filesystem::absolute(catalogPath).wstring();

    {
        wit::app::CatalogWorkflowController controller;
        auto createResult = controller.CreateCatalogPathSelected(catalogPath.wstring());
        ASSERT_TRUE(createResult.messages.empty()) << "catalog creates through app controller";
        ASSERT_FALSE(createResult.browserEffects.empty()) << "create publishes browser effect";

        auto savedAfterCreate = wit::platform::LoadAppSettings();
        ASSERT_EQ(savedAfterCreate.lastCatalogPath, normalizedCatalogPath);
        ASSERT_FALSE(savedAfterCreate.recentCatalogPaths.empty());
        ASSERT_EQ(savedAfterCreate.recentCatalogPaths.front(), normalizedCatalogPath);

        auto requestClose = controller.RequestCloseCatalog();
        ASSERT_EQ(requestClose.request.kind, wit::app::RequestKind::ConfirmCloseCatalog);

        auto closeResult = controller.AnswerCloseCatalog(IDYES);
        ASSERT_TRUE(closeResult.messages.empty()) << "closing catalog saves startup settings";
        ASSERT_TRUE(closeResult.presentation.refreshBrowserStatus);

        auto savedAfterClose = wit::platform::LoadAppSettings();
        EXPECT_TRUE(savedAfterClose.lastCatalogPath.empty());
        ASSERT_FALSE(savedAfterClose.recentCatalogPaths.empty());
        EXPECT_EQ(savedAfterClose.recentCatalogPaths.front(), normalizedCatalogPath);

        auto appCloseResult = controller.RequestWindowClose();
        EXPECT_TRUE(appCloseResult.destroyWindow);
    }

    std::filesystem::remove_all(testRoot);
}
