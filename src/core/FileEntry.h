#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace wit::core {
struct FileEntry {
    std::int64_t id{};
    std::int64_t catalogId{};
    std::int64_t folderId{};
    std::wstring parentPath;
    std::wstring name;
    std::wstring description;
    std::wstring extension;
    std::optional<std::wstring> crc;
    std::uint64_t size{};
    std::int64_t createdAt{};
    std::int64_t modifiedAtValue{};
    std::int64_t accessedAt{};
    std::wstring modifiedAt;
    std::uint32_t attributes{};
    bool isDirectory{};
};
}
