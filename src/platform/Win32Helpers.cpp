#include "Win32Helpers.h"
#include <algorithm>
#include <cstdint>

namespace wit::platform {
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
static std::wstring SystemTimeToIso(const SYSTEMTIME& st) {
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02uZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buffer;
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
std::wstring FormatUnixTimestamp(std::int64_t timestamp) {
    constexpr std::uint64_t epochDifference = 116444736000000000ULL;
    ULARGE_INTEGER value{};
    value.QuadPart = epochDifference + static_cast<std::uint64_t>((std::max)(timestamp, std::int64_t{})) * 10000000ULL;
    FILETIME fileTime{value.LowPart, value.HighPart};
    SYSTEMTIME utc{};
    if (!FileTimeToSystemTime(&fileTime, &utc)) return {};
    return SystemTimeToIso(utc);
}
}
