#pragma once
#include <cstdint>
#include <string>

namespace wit::core {
struct FolderEntry {
    std::int64_t id{};
    std::int64_t diskId{};
    std::int64_t parentFolderId{};
    bool hasParent{};
    std::wstring path;
    std::wstring name;
    std::int64_t createdAt{};
    std::int64_t modifiedAt{};
    std::int64_t accessedAt{};
    std::uint32_t attributes{};
    std::uint64_t contentSize{};
};
}
