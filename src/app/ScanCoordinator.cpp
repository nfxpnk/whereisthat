#include "ScanCoordinator.h"
#include <memory>
#include <string>
#include <vector>
#include "../core/FileScanner.h"
#include "../platform/VolumeInfo.h"
#include "../platform/Win32Helpers.h"

namespace wit::app {
namespace {

std::wstring NameForCatalogRoot(const std::wstring& root) {
    if (root.size() == 3 && root[1] == L':' && (root[2] == L'\\' || root[2] == L'/')) return root;
    const auto end = root.find_last_not_of(L"\\/");
    if (end == std::wstring::npos) return root;
    const auto separator = root.find_last_of(L"\\/", end);
    return separator == std::wstring::npos ? root.substr(0, end + 1) :
        root.substr(separator + 1, end - separator);
}

std::wstring NormalizedSourcePath(const std::wstring& path) {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
        if (length == 0) return path;
        if (length < buffer.size()) {
            std::wstring normalized(buffer.data(), length);
            while (normalized.size() > 3 && (normalized.back() == L'\\' || normalized.back() == L'/')) {
                normalized.pop_back();
            }
            return normalized;
        }
        buffer.resize(static_cast<std::size_t>(length) + 1);
    }
}

}

ScanCoordinator::~ScanCoordinator() {
    Join();
}

void ScanCoordinator::Join() {
    if (worker_.joinable()) worker_.join();
}

bool ScanCoordinator::Start(HWND target, wit::storage::Database* source,
    const wit::ui::AddNewDiskMediaResult& media) {
    if (IsRunning() || !source) return false;
    auto candidate = std::make_unique<wit::storage::Database>();
    if (!candidate->CreateWorkingCopy(*source)) return false;
    const std::wstring root = NormalizedSourcePath(media.scanRoot);
    const std::wstring diskName = media.diskName.empty() ? NameForCatalogRoot(root) : media.diskName;
    std::int64_t diskNumber{};
    try {
        if (!media.diskNumber.empty()) diskNumber = std::stoll(media.diskNumber);
    } catch (...) {
        diskNumber = 0;
    }
    auto* staged = candidate.release();
    worker_ = std::thread([target, root, diskName, diskNumber, media, staged]() {
        std::int64_t id{};
        bool success = staged->BeginTransaction();
        wit::core::Disk disk;
        disk.diskName = diskName;
        disk.diskNumber = diskNumber;
        disk.sourcePath = root;
        disk.location = media.originalPath;
        disk.diskType = media.kind == wit::ui::MediaSourceKind::Iso
            ? wit::core::DiskType::VirtualDisk : wit::core::DiskType::Other;
        disk.addedAt = wit::platform::NowUnixSeconds();
        disk.updatedAt = disk.addedAt;
        wit::platform::PopulateVolumeMetadata(root, disk);
        if (success) {
            id = staged->FindDiskBySourcePath(root,
                media.kind == wit::ui::MediaSourceKind::Iso ? media.originalPath : L"");
            if (id != 0) {
                disk.id = id;
                success = staged->DeleteContentForDisk(id);
            } else {
                id = staged->AddDisk(disk);
                disk.id = id;
                success = id != 0;
            }
        }
        if (success) {
            wit::core::FileScanner scanner;
            wit::core::FileScanner::Result scanResult;
            success = scanner.ScanFolder(root, id, *staged,
                [target](const wit::core::FileScanner::Progress& progress) {
                    PostMessageW(target, WM_SCAN_PROGRESS, 0, reinterpret_cast<LPARAM>(
                        new ScanProgressMessage{progress.scannedFiles, progress.scannedFolders}));
                }, scanResult, media.calculateCrc, false);
            if (success) {
                disk.totalFiles = static_cast<std::int64_t>(scanResult.totalFiles);
                disk.totalFolders = static_cast<std::int64_t>(scanResult.totalFolders);
                disk.updatedAt = wit::platform::NowUnixSeconds();
                wit::core::DiskScanStatistics statistics;
                statistics.diskId = id;
                statistics.lastScannedAt = disk.updatedAt;
                statistics.imageScanningTimeMs = scanResult.elapsedMilliseconds;
                statistics.calculatedFileCrcs = media.calculateCrc;
                success = staged->UpdateDisk(disk) && staged->UpdateDiskScanStatistics(statistics);
            }
        }
        if (success) {
            if (!staged->Commit()) {
                staged->Rollback();
                success = false;
            }
        } else if (staged->IsOpen()) {
            staged->Rollback();
        }
        PostMessageW(target, WM_SCAN_COMPLETE, 0, reinterpret_cast<LPARAM>(
            new ScanCompleteMessage{staged, success}));
    });
    return true;
}

}
