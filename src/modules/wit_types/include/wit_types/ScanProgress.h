#pragma once
#include <cstdint>

namespace wit::app {
struct ScanProgress {
    std::uint64_t files{};
    std::uint64_t folders{};
    std::uint64_t totalFiles{};
    std::uint64_t remainingFiles{};
    bool totalKnown{};
    bool counting{};
};
}

