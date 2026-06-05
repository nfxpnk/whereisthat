#include "wit_infra/StringUtils.h"
#include <format>

namespace wit::core {
std::wstring FormatSize(std::uint64_t bytes) {
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    int index = 0;
    while (value >= 1024 && index < 4) {
        value /= 1024;
        ++index;
    }
    return std::format(L"{:.2f} {}", value, units[index]);
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

