#pragma once

#include <Windows.h>
#include <cstdint>
#include <thread>
#include "../storage/Database.h"
#include "../ui/AddNewDiskMediaDialog.h"

namespace wit::app {

constexpr UINT WM_SCAN_PROGRESS = WM_APP + 1;
constexpr UINT WM_SCAN_COMPLETE = WM_APP + 2;

struct ScanProgressMessage {
    std::uint64_t files{};
    std::uint64_t folders{};
};

struct ScanCompleteMessage {
    wit::storage::Database* pending{};
    bool success{};
};

class ScanCoordinator {
public:
    ~ScanCoordinator();
    bool IsRunning() const { return worker_.joinable(); }
    bool Start(HWND target, wit::storage::Database* source, const wit::ui::AddNewDiskMediaResult& media);
    void Join();

private:
    std::thread worker_;
};

}
