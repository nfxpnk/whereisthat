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

void FormatSizeRawBytesToBuffer(std::uint64_t bytes, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    if (bytes == 0) {
        swprintf_s(buffer, bufferSize, L"0");
        return;
    }
    // Format with comma separators: 111,222,333
    wchar_t raw[64];
    swprintf_s(raw, std::size(raw), L"%llu", static_cast<unsigned long long>(bytes));
    wchar_t* write = buffer;
    const std::size_t len = wcslen(raw);
    // Position of first comma (groups of 3 from right)
    const std::size_t firstGroup = len % 3;
    for (std::size_t i = 0; i < len && (write - buffer) + 1 < bufferSize; ++i) {
        if (i > 0 && (i % 3) == firstGroup) {
            if (write - buffer + 1 < bufferSize) *write++ = L',';
        }
        if (write - buffer < bufferSize) *write++ = raw[i];
    }
    if (write - buffer < bufferSize) *write = L'\0';
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

