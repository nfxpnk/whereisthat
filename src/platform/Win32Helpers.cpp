#include "Win32Helpers.h"
#include <chrono>
#include <iomanip>
#include <sstream>

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
std::wstring FileTimeToIso8601(const FILETIME& fileTime) {
    SYSTEMTIME utc{};
    FileTimeToSystemTime(&fileTime, &utc);
    return SystemTimeToIso(utc);
}
std::wstring NowIso8601() {
    SYSTEMTIME utc{};
    GetSystemTime(&utc);
    return SystemTimeToIso(utc);
}
}
