#pragma once
#include <cstdint>
#include <string>

namespace wit::core {
struct FileEntry {
    std::int64_t id{};
    std::int64_t catalogId{};
    std::wstring parentPath;
    std::wstring name;
    std::wstring extension;
    std::uint64_t size{};
    std::wstring modifiedAt;
    std::uint32_t attributes{};
};
}
