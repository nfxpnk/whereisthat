#include <wit_infra/Win32Helpers.h>
#include <algorithm>
#include <cstdint>
#include <format>
#include <mutex>
#include <cwchar>
#include <string_view>

namespace wit::platform {
namespace {

std::mutex g_dateTimeFormatMutex;
std::wstring g_dateTimeFormatOverride;

static bool UnixTimestampToLocalSystemTime(std::int64_t timestamp, SYSTEMTIME& local) {
    constexpr std::uint64_t epochDifference = 116444736000000000ULL;
    ULARGE_INTEGER value{};
    value.QuadPart = epochDifference + static_cast<std::uint64_t>((std::max)(timestamp, std::int64_t{})) * 10000000ULL;
    FILETIME fileTime{value.LowPart, value.HighPart};
    FILETIME localFileTime{};
    return FileTimeToLocalFileTime(&fileTime, &localFileTime) != FALSE &&
        FileTimeToSystemTime(&localFileTime, &local) != FALSE;
}

static std::wstring FormatWithWindowsLocale(const SYSTEMTIME& local) {
    std::wstring date(128, L'\0');
    int dateLength = GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr,
        date.data(), static_cast<int>(date.size()), nullptr);
    if (dateLength == 0) return {};
    date.resize(static_cast<std::size_t>(dateLength - 1));

    std::wstring time(128, L'\0');
    const int timeLength = GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_FORCE24HOURFORMAT, &local, nullptr,
        time.data(), static_cast<int>(time.size()));
    if (timeLength == 0) return date;
    time.resize(static_cast<std::size_t>(timeLength - 1));
    return date + L" " + time;
}

static void AppendNumber(std::wstring& output, int value, int width) {
    auto text = std::format(L"{}", value);
    if (width > 1 && text.size() < static_cast<std::size_t>(width)) {
        output.append(static_cast<std::size_t>(width) - text.size(), L'0');
    }
    output += text;
}

static bool IsPatternSeparator(wchar_t character) {
    return character == L' ' || character == L'-' || character == L'/' ||
        character == L'.' || character == L':' || character == L'\\';
}

static std::wstring FormatWithPattern(const SYSTEMTIME& local, std::wstring_view pattern) {
    std::wstring output;
    output.reserve(pattern.size() + 8);
    for (std::size_t index = 0; index < pattern.size();) {
        const auto remaining = pattern.substr(index);
        if (remaining.starts_with(L"YYYY")) {
            AppendNumber(output, local.wYear, 4);
            index += 4;
        } else if (remaining.starts_with(L"YY")) {
            AppendNumber(output, local.wYear % 100, 2);
            index += 2;
        } else if (remaining.starts_with(L"MM")) {
            AppendNumber(output, local.wMonth, 2);
            index += 2;
        } else if (remaining.starts_with(L"DD")) {
            AppendNumber(output, local.wDay, 2);
            index += 2;
        } else if (remaining.starts_with(L"HH")) {
            AppendNumber(output, local.wHour, 2);
            index += 2;
        } else if (remaining.starts_with(L"mm")) {
            AppendNumber(output, local.wMinute, 2);
            index += 2;
        } else if (remaining.starts_with(L"ss")) {
            AppendNumber(output, local.wSecond, 2);
            index += 2;
        } else if (remaining.starts_with(L"M")) {
            AppendNumber(output, local.wMonth, 1);
            ++index;
        } else if (remaining.starts_with(L"D")) {
            AppendNumber(output, local.wDay, 1);
            ++index;
        } else if (remaining.starts_with(L"H")) {
            AppendNumber(output, local.wHour, 1);
            ++index;
        } else if (remaining.starts_with(L"m")) {
            AppendNumber(output, local.wMinute, 1);
            ++index;
        } else if (remaining.starts_with(L"s")) {
            AppendNumber(output, local.wSecond, 1);
            ++index;
        } else {
            output.push_back(pattern[index]);
            ++index;
        }
    }
    return output;
}

static std::wstring CurrentPattern() {
    std::scoped_lock lock(g_dateTimeFormatMutex);
    return g_dateTimeFormatOverride;
}

}

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), size, nullptr, nullptr);
    return out;
}
std::wstring ToUtf16(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
    return out;
}
std::int64_t FileTimeToUnixSeconds(const FILETIME& fileTime) {
    ULARGE_INTEGER value{};
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;
    constexpr std::uint64_t epochDifference = 116444736000000000ULL;
    return value.QuadPart < epochDifference ? 0 :
        static_cast<std::int64_t>((value.QuadPart - epochDifference) / 10000000ULL);
}
std::int64_t NowUnixSeconds() {
    FILETIME fileTime{};
    GetSystemTimeAsFileTime(&fileTime);
    return FileTimeToUnixSeconds(fileTime);
}

void SetDateTimeFormatOverride(const std::wstring& pattern) {
    std::scoped_lock lock(g_dateTimeFormatMutex);
    g_dateTimeFormatOverride = pattern;
}

std::wstring DateTimeFormatOverride() {
    return CurrentPattern();
}

bool IsValidDateTimeFormat(const std::wstring& pattern) {
    if (pattern.empty()) return true;
    bool hasYear{};
    bool hasMonth{};
    bool hasDay{};
    bool hasHour{};
    bool hasMinute{};
    bool hasSecond{};
    for (std::size_t index = 0; index < pattern.size();) {
        const auto remaining = std::wstring_view(pattern).substr(index);
        if (remaining.starts_with(L"YYYY")) {
            hasYear = true;
            index += 4;
        } else if (remaining.starts_with(L"YY")) {
            hasYear = true;
            index += 2;
        } else if (remaining.starts_with(L"MM")) {
            hasMonth = true;
            index += 2;
        } else if (remaining.starts_with(L"DD")) {
            hasDay = true;
            index += 2;
        } else if (remaining.starts_with(L"HH")) {
            hasHour = true;
            index += 2;
        } else if (remaining.starts_with(L"mm")) {
            hasMinute = true;
            index += 2;
        } else if (remaining.starts_with(L"ss")) {
            hasSecond = true;
            index += 2;
        } else if (remaining.starts_with(L"M")) {
            hasMonth = true;
            ++index;
        } else if (remaining.starts_with(L"D")) {
            hasDay = true;
            ++index;
        } else if (remaining.starts_with(L"H")) {
            hasHour = true;
            ++index;
        } else if (remaining.starts_with(L"m")) {
            hasMinute = true;
            ++index;
        } else if (remaining.starts_with(L"s")) {
            hasSecond = true;
            ++index;
        } else if (IsPatternSeparator(pattern[index])) {
            ++index;
        } else {
            return false;
        }
    }
    return hasYear && hasMonth && hasDay && hasHour && hasMinute && hasSecond;
}

std::wstring FormatDateTimeSample(const std::wstring& pattern) {
    SYSTEMTIME sample{2022, 1, 1, 31, 13, 14, 22, 0};
    return pattern.empty() ? FormatWithWindowsLocale(sample) : FormatWithPattern(sample, pattern);
}

std::wstring FormatUnixTimestamp(std::int64_t timestamp) {
    SYSTEMTIME local{};
    if (!UnixTimestampToLocalSystemTime(timestamp, local)) return {};
    const auto pattern = CurrentPattern();
    return pattern.empty() ? FormatWithWindowsLocale(local) : FormatWithPattern(local, pattern);
}
void FormatUnixTimestampToBuffer(std::int64_t timestamp, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    buffer[0] = L'\0';
    SYSTEMTIME local{};
    if (!UnixTimestampToLocalSystemTime(timestamp, local)) return;
    const auto pattern = CurrentPattern();
    if (pattern.empty()) {
        const int dateLength = GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr,
            buffer, static_cast<int>(bufferSize), nullptr);
        if (dateLength == 0) return;
        const auto used = static_cast<std::size_t>(dateLength - 1);
        if (used + 1 >= bufferSize) return;
        buffer[used] = L' ';
        const int timeLength = GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_FORCE24HOURFORMAT, &local, nullptr,
            buffer + used + 1, static_cast<int>(bufferSize - used - 1));
        if (timeLength == 0) buffer[used] = L'\0';
        return;
    }
    const auto formatted = FormatWithPattern(local, pattern);
    swprintf_s(buffer, bufferSize, L"%s", formatted.c_str());
}
std::wstring FormatUnixDate(std::int64_t timestamp) {
    return FormatUnixTimestamp(timestamp);
}
}
