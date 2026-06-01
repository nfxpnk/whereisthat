#pragma once
#include <cstdint>
#include <string>

namespace wit::core {
struct BrowserLocation {
    bool isRoot{true};
    std::int64_t sourceId{};
    std::wstring sourceName;
    std::wstring sourceRoot;
    std::wstring path;

    bool operator==(const BrowserLocation& other) const {
        return isRoot == other.isRoot && sourceId == other.sourceId && path == other.path;
    }
};
}

