#pragma once
#include <cstdint>
#include <string>

namespace wit::search {
struct SearchResult {
    std::int64_t id{};
    std::wstring name;
    std::wstring path;
    std::uint64_t size{};
};
}

