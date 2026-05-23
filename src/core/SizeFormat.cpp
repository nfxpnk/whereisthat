#include "SizeFormat.h"
#include <cwchar>

namespace wit::core {
std::wstring FormatSize(std::uint64_t bytes) {
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    int index = 0;
    while (value >= 1024 && index < 4) {
        value /= 1024;
        ++index;
    }
    wchar_t buffer[64];
    swprintf_s(buffer, L"%.2f %s", value, units[index]);
    return buffer;
}
}
