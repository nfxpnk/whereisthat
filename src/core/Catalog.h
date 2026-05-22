#pragma once
#include <cstdint>
#include <string>

namespace wit::core {
struct Catalog {
    std::int64_t id{};
    std::wstring name;
    std::wstring rootPath;
    std::wstring createdAt;
    std::int64_t itemCount{};
};
}
