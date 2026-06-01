#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace wit::database {
using SqlParam = std::variant<std::nullptr_t, std::int64_t, std::wstring>;
}

