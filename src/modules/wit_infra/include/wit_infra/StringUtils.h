#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace wit::core {
std::wstring FormatSize(std::uint64_t bytes);
void FormatSizeToBuffer(std::uint64_t bytes, wchar_t* buffer, std::size_t bufferSize);
std::string_view TrimAsciiWhitespace(std::string_view text);
}
