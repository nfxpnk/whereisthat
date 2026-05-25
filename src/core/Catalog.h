#pragma once
#include <cstdint>
#include <string>

namespace wit::core {
struct Catalog {
    std::int64_t id{};
    std::wstring name;
    std::wstring rootPath;
    std::int64_t addedAt{};
    std::int64_t totalFiles{};
    std::int64_t totalFolders{};
};
}
