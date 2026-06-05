#include "wit_infra/StringUtils.h"
#include <format>
#include <cwchar>
#include <utility>

namespace wit::core {
namespace {
const wchar_t* const kSizeUnits[] = {L"B", L"KB", L"MB", L"GB", L"TB"};

std::pair<double, int> ScaledSize(std::uint64_t bytes) {
    double value = static_cast<double>(bytes);
    int index = 0;
    while (value >= 1024 && index < 4) {
        value /= 1024;
        ++index;
    }
    return {value, index};
}
}

std::wstring FormatSize(std::uint64_t bytes) {
    const auto [value, index] = ScaledSize(bytes);
    return std::format(L"{:.2f} {}", value, kSizeUnits[index]);
}

void FormatSizeToBuffer(std::uint64_t bytes, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    const auto [value, index] = ScaledSize(bytes);
    swprintf_s(buffer, bufferSize, L"%.2f %s", value, kSizeUnits[index]);
}

std::string_view TrimAsciiWhitespace(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' ||
        text.front() == '\r' || text.front() == '\n')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' ||
        text.back() == '\r' || text.back() == '\n')) {
        text.remove_suffix(1);
    }
    return text;
}
}

