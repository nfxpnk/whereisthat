#pragma once
#include <cstdint>
#include <string>

namespace wit::core {
enum class FolderEntryType {
    Directory,
    Archive
};

inline const char* FolderEntryTypeValue(FolderEntryType type) {
    return type == FolderEntryType::Archive ? "archive" : "directory";
}

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
    FolderEntryType entryType{FolderEntryType::Directory};
};
}

