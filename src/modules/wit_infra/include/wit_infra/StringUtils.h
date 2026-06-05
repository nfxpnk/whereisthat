#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace wit::core {
std::wstring FormatSize(std::uint64_t bytes);
std::string_view TrimAsciiWhitespace(std::string_view text);
}

