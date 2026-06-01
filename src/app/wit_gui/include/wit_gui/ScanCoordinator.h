#pragma once

#include <Windows.h>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "wit_types/CatalogIdentity.h"
#include "wit_database/Database.h"
#include "wit_scanner/ScanRequest.h"
#include "wit_types/ScanProgress.h"

namespace wit::app {

constexpr UINT WM_SCAN_PROGRESS = WM_APP + 1;
constexpr UINT WM_SCAN_COMPLETE = WM_APP + 2;
using ScanId = std::uintptr_t;

enum class ScanOutcome {
    Completed,
    Failed,
    Cancelled
};

struct ScanResult {
    ScanId id{};
    wit::core::CatalogId destinationCatalogId{};
    ScanOutcome outcome{ScanOutcome::Failed};
    std::unique_ptr<wit::storage::Database> pending;
    std::wstring error;
};

// Public methods are owner/UI-thread only. Worker threads communicate through
// mailboxMutex_ and posted Win32 messages.
class ScanCoordinator {
public:
    ScanCoordinator();
    ~ScanCoordinator();
    ScanCoordinator(const ScanCoordinator&) = delete;
    ScanCoordinator& operator=(const ScanCoordinator&) = delete;

    bool AttachTarget(HWND target);
    void DetachTarget();
    bool IsRunning() const;
    bool IsCancelling() const;
    bool Targets(wit::core::CatalogId id) const;
    bool Start(wit::storage::Database* source, const wit::core::ScanRequest& request, ScanId& scanId);
    bool RequestCancel();
    std::optional<ScanProgress> TakeProgress(ScanId scanId);
    std::optional<ScanResult> TakeResult(ScanId scanId);
    void RetireWorker(ScanId scanId);

private:
    void AssertOwnerThread() const;
    static LRESULT CALLBACK DeliveryWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT DispatchNotification(UINT message, WPARAM wparam, LPARAM lparam);
    void RunScan(std::stop_token stopToken, ScanId scanId, std::wstring root, std::wstring diskName,
        std::int64_t diskNumber, wit::core::ScanRequest request,
        std::unique_ptr<wit::storage::Database> staged);
    void PublishProgress(ScanId scanId, const ScanProgress& progress);
    void PublishResult(ScanResult result);
    void PostNotification(UINT message, ScanId scanId) const;
    void ReapWorkers();

    HWND deliveryWindow_{};
    HWND targetWindow_{};
    DWORD ownerThreadId_{GetCurrentThreadId()};
    ScanId nextScanId_{1};
    ScanId activeScanId_{};
    wit::core::CatalogId activeCatalogId_{};
    bool cancellationRequested_{};
    std::jthread worker_;

    std::mutex mailboxMutex_;
    std::unordered_map<ScanId, ScanProgress> progress_;
    std::unordered_map<ScanId, ScanResult> results_;

    std::mutex reaperMutex_;
    std::condition_variable reaperCondition_;
    std::vector<std::jthread> retiredWorkers_;
    bool stopReaper_{};
    std::jthread reaper_;
};

}



