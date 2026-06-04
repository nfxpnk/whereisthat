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
}

