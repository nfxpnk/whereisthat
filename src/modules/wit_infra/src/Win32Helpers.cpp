#include <wit_infra/Win32Helpers.h>
#include <algorithm>
#include <cstdint>
#include <format>

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
    return std::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}
static bool UnixTimestampToSystemTime(std::int64_t timestamp, SYSTEMTIME& utc) {
    constexpr std::uint64_t epochDifference = 116444736000000000ULL;
    ULARGE_INTEGER value{};
    value.QuadPart = epochDifference + static_cast<std::uint64_t>((std::max)(timestamp, std::int64_t{})) * 10000000ULL;
    FILETIME fileTime{value.LowPart, value.HighPart};
    return FileTimeToSystemTime(&fileTime, &utc) != FALSE;
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
    SYSTEMTIME utc{};
    if (!UnixTimestampToSystemTime(timestamp, utc)) return {};
    return SystemTimeToIso(utc);
}
std::wstring FormatUnixDate(std::int64_t timestamp) {
    SYSTEMTIME utc{};
    if (!UnixTimestampToSystemTime(timestamp, utc)) return {};
    return std::format(L"{}/{:02}/{:04}", utc.wMonth, utc.wDay, utc.wYear);
}
}
