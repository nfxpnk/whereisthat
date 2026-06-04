#include <wit_gui/ScanCoordinator.h>
#include <cassert>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include "wit_scanner/FileScanner.h"
#include <wit_infra/PathHelpers.h>
#include <wit_infra/VolumeInfo.h>
#include <wit_infra/Win32Helpers.h>

namespace wit::app {
namespace {

constexpr wchar_t kScanDeliveryWindowClass[] = L"WhereIsThatScanDeliveryWindow";

}

ScanCoordinator::ScanCoordinator() : reaper_([this]() { ReapWorkers(); }) {
}

ScanCoordinator::~ScanCoordinator() {
    DetachTarget();
    // Best-effort shutdown cleanup before joining the worker.
    (void)RequestCancel();
    if (worker_.joinable()) worker_.join();
    {
        std::scoped_lock lock(reaperMutex_);
        stopReaper_ = true;
    }
    reaperCondition_.notify_one();
    if (reaper_.joinable()) reaper_.join();
    if (deliveryWindow_) DestroyWindow(deliveryWindow_);
}

void ScanCoordinator::AssertOwnerThread() const {
    assert(ownerThreadId_ == GetCurrentThreadId());
}

bool ScanCoordinator::AttachTarget(HWND target) {
    AssertOwnerThread();
    if (deliveryWindow_) {
        targetWindow_ = target;
        return true;
    }
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = DeliveryWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kScanDeliveryWindowClass;
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    deliveryWindow_ = CreateWindowExW(0, kScanDeliveryWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
        windowClass.hInstance, this);
    if (!deliveryWindow_) return false;
    targetWindow_ = target;
    return true;
}

void ScanCoordinator::DetachTarget() {
    AssertOwnerThread();
    targetWindow_ = nullptr;
}

bool ScanCoordinator::IsRunning() const {
    AssertOwnerThread();
    return activeScanId_ != 0;
}

bool ScanCoordinator::IsCancelling() const {
    AssertOwnerThread();
    return cancellationRequested_;
}

bool ScanCoordinator::Targets(wit::core::CatalogId id) const {
    AssertOwnerThread();
    return activeScanId_ != 0 && activeCatalogId_ == id;
}

bool ScanCoordinator::Start(wit::storage::Database* source, const wit::core::ScanRequest& request,
    ScanId& scanId) {
    AssertOwnerThread();
    if (IsRunning() || !deliveryWindow_ || !source) return false;
    auto candidate = std::make_unique<wit::storage::Database>();
    if (!candidate->CreateWorkingCopy(*source)) return false;
    const auto rootPath = std::filesystem::absolute(request.scanRoot);
    const std::wstring root = rootPath.wstring();
    const std::wstring diskName = request.diskName.empty()
        ? wit::platform::DisplayNameForPath(rootPath) : request.diskName;
    std::int64_t diskNumber{};
    try {
        if (!request.diskNumber.empty()) diskNumber = std::stoll(request.diskNumber);
    } catch (...) {
        diskNumber = 0;
    }
    scanId = nextScanId_++;
    if (scanId == 0) scanId = nextScanId_++;
    activeScanId_ = scanId;
    activeCatalogId_ = request.destinationCatalogId;
    cancellationRequested_ = false;
    worker_ = std::jthread([this, scanId, root, diskName, diskNumber, request,
        staged = std::move(candidate)](std::stop_token stopToken) mutable {
        try {
            RunScan(stopToken, scanId, root, diskName, diskNumber, request, std::move(staged));
        } catch (const std::exception& error) {
            ScanResult result;
            result.id = scanId;
            result.destinationCatalogId = request.destinationCatalogId;
            result.outcome = ScanOutcome::Failed;
            result.error = L"The scan failed unexpectedly. The saved catalog was not changed.";
            if (error.what() && error.what()[0] != '\0') {
                result.error += L" ";
                result.error += wit::platform::ToUtf16(error.what());
            }
            PublishResult(std::move(result));
        } catch (...) {
            ScanResult result;
            result.id = scanId;
            result.destinationCatalogId = request.destinationCatalogId;
            result.outcome = ScanOutcome::Failed;
            result.error = L"The scan failed unexpectedly. The saved catalog was not changed.";
            PublishResult(std::move(result));
        }
    });
    return true;
}

bool ScanCoordinator::RequestCancel() {
    AssertOwnerThread();
    if (!IsRunning()) return false;
    cancellationRequested_ = true;
    worker_.request_stop();
    return true;
}

std::optional<ScanProgress> ScanCoordinator::TakeProgress(ScanId scanId) {
    AssertOwnerThread();
    std::scoped_lock lock(mailboxMutex_);
    const auto found = progress_.find(scanId);
    if (found == progress_.end()) return std::nullopt;
    auto progress = found->second;
    progress_.erase(found);
    return progress;
}

std::optional<ScanResult> ScanCoordinator::TakeResult(ScanId scanId) {
    AssertOwnerThread();
    std::scoped_lock lock(mailboxMutex_);
    const auto found = results_.find(scanId);
    if (found == results_.end()) return std::nullopt;
    auto result = std::move(found->second);
    results_.erase(found);
    progress_.erase(scanId);
    return result;
}

void ScanCoordinator::RetireWorker(ScanId scanId) {
    AssertOwnerThread();
    if (scanId != activeScanId_) return;
    activeScanId_ = 0;
    activeCatalogId_ = 0;
    cancellationRequested_ = false;
    if (!worker_.joinable()) return;
    {
        std::scoped_lock lock(reaperMutex_);
        retiredWorkers_.push_back(std::move(worker_));
    }
    reaperCondition_.notify_one();
}

LRESULT CALLBACK ScanCoordinator::DeliveryWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<ScanCoordinator*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<ScanCoordinator*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && (message == WM_SCAN_PROGRESS || message == WM_SCAN_COMPLETE)) {
        return self->DispatchNotification(message, wparam, lparam);
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT ScanCoordinator::DispatchNotification(UINT message, WPARAM wparam, LPARAM lparam) {
    if (targetWindow_) SendMessageW(targetWindow_, message, wparam, lparam);
    return 0;
}

void ScanCoordinator::RunScan(std::stop_token stopToken, ScanId scanId, std::wstring root, std::wstring diskName,
    std::int64_t diskNumber, wit::core::ScanRequest request,
    std::unique_ptr<wit::storage::Database> staged) {
    ScanResult result;
    result.id = scanId;
    result.destinationCatalogId = request.destinationCatalogId;
    result.outcome = ScanOutcome::Failed;
    result.error = L"The scan could not be staged. The saved catalog was not changed.";
    const auto cancelled = [&]() {
        if (!stopToken.stop_requested()) return false;
        if (staged->IsOpen()) {
            // Best-effort cleanup; the cancellation result is already recorded below.
            (void)staged->Rollback();
        }
        result.outcome = ScanOutcome::Cancelled;
        result.error.clear();
        return true;
    };

    wit::core::FileScanner scanner;
    std::uint64_t totalFiles{};
    PublishProgress(scanId, {0, 0, 0, 0, false, true});
    bool success = !cancelled() && scanner.CountFiles(root, totalFiles, stopToken);
    if (success && !cancelled()) PublishProgress(scanId, {0, 0, totalFiles, totalFiles, true, false});

    std::int64_t id{};
    success = success && !cancelled() && staged->BeginTransaction();
    wit::core::Disk disk;
    disk.diskGroupId = request.diskGroupId;
    disk.diskName = diskName;
    disk.diskNumber = diskNumber;
    disk.sourcePath = root;
    disk.location = request.originalPath;
    disk.diskType = request.kind == wit::core::MediaSourceKind::Iso
        ? wit::core::DiskType::VirtualDisk : wit::core::DiskType::Other;
    disk.addedAt = wit::platform::NowUnixSeconds();
    disk.updatedAt = disk.addedAt;
    if (success && !cancelled()) wit::platform::PopulateVolumeMetadata(root, disk);
    if (success && !cancelled()) {
        id = staged->FindDiskBySourcePath(root,
            request.kind == wit::core::MediaSourceKind::Iso ? request.originalPath : L"");
        if (id != 0) {
            disk.id = id;
            success = staged->DeleteContentForDisk(id);
        } else {
            id = staged->AddDisk(disk);
            disk.id = id;
            success = id != 0;
        }
    }
    if (success && !cancelled()) {
        wit::core::FileScanner::Result scanResult;
        success = scanner.ScanFolder(root, id, *staged,
            [this, scanId, totalFiles](const wit::core::FileScanner::Progress& progress) {
                const auto remaining = progress.scannedFiles < totalFiles
                    ? totalFiles - progress.scannedFiles : 0;
                PublishProgress(scanId, {progress.scannedFiles, progress.scannedFolders, totalFiles,
                    remaining, true, false});
            }, scanResult, request.calculateCrc, false, stopToken, request.browseArchives);
        if (success && !cancelled()) {
            disk.totalFiles = static_cast<std::int64_t>(scanResult.totalFiles);
            disk.totalFolders = static_cast<std::int64_t>(scanResult.totalFolders);
            disk.updatedAt = wit::platform::NowUnixSeconds();
            wit::core::DiskScanStatistics statistics;
            statistics.diskId = id;
            statistics.lastScannedAt = disk.updatedAt;
            statistics.imageScanningTimeMs = scanResult.elapsedMilliseconds;
            statistics.calculatedFileCrcs = request.calculateCrc;
            statistics.scannedArchives = static_cast<std::int64_t>(scanResult.scannedArchives);
            statistics.archiveFilesCount = static_cast<std::int64_t>(scanResult.archiveFiles);
            statistics.archiveFoldersCount = static_cast<std::int64_t>(scanResult.archiveFolders);
            success = staged->UpdateDisk(disk) && staged->UpdateDiskScanStatistics(statistics);
        }
    }
    if (!cancelled() && success) {
        if (staged->Commit()) {
            if (stopToken.stop_requested()) {
                result.outcome = ScanOutcome::Cancelled;
                result.error.clear();
            } else {
                result.outcome = ScanOutcome::Completed;
                result.error.clear();
                result.pending = std::move(staged);
            }
        } else {
            // Best-effort cleanup; the commit failure remains the reported error.
            (void)staged->Rollback();
        }
    } else if (result.outcome != ScanOutcome::Cancelled && staged->IsOpen()) {
        // Best-effort cleanup; the earlier failure remains the reported error.
        (void)staged->Rollback();
    }
    PublishResult(std::move(result));
}

void ScanCoordinator::PublishProgress(ScanId scanId, const ScanProgress& progress) {
    {
        std::scoped_lock lock(mailboxMutex_);
        progress_[scanId] = progress;
    }
    PostNotification(WM_SCAN_PROGRESS, scanId);
}

void ScanCoordinator::PublishResult(ScanResult result) {
    const auto scanId = result.id;
    {
        std::scoped_lock lock(mailboxMutex_);
        progress_.erase(scanId);
        results_.insert_or_assign(scanId, std::move(result));
    }
    PostNotification(WM_SCAN_COMPLETE, scanId);
}

void ScanCoordinator::PostNotification(UINT message, ScanId scanId) const {
    if (deliveryWindow_) PostMessageW(deliveryWindow_, message, static_cast<WPARAM>(scanId), 0);
}

void ScanCoordinator::ReapWorkers() {
    for (;;) {
        std::jthread retired;
        {
            std::unique_lock lock(reaperMutex_);
            reaperCondition_.wait(lock, [this]() { return stopReaper_ || !retiredWorkers_.empty(); });
            if (retiredWorkers_.empty()) {
                if (stopReaper_) return;
                continue;
            }
            retired = std::move(retiredWorkers_.back());
            retiredWorkers_.pop_back();
        }
        if (retired.joinable()) retired.join();
    }
}

}

