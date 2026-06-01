#pragma once
#include <cstdint>
#include <string>

namespace wit::core {
struct BrowserLocation {
    bool isRoot{true};
    bool isDiskGroup{};
    std::int64_t diskGroupId{};
    std::wstring diskGroupName;
    std::int64_t sourceId{};
    std::wstring sourceName;
    std::wstring sourceRoot;
    std::wstring path;

    bool operator==(const BrowserLocation& other) const {
        return isRoot == other.isRoot && isDiskGroup == other.isDiskGroup &&
            diskGroupId == other.diskGroupId && sourceId == other.sourceId && path == other.path;
    }
};
}

