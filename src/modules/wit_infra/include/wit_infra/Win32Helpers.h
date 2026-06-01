#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

namespace wit::platform {
std::string ToUtf8(const std::wstring& value);
std::wstring ToUtf16(const std::string& value);
std::int64_t FileTimeToUnixSeconds(const FILETIME& fileTime);
std::int64_t NowUnixSeconds();
std::wstring FormatUnixTimestamp(std::int64_t timestamp);
std::wstring FormatUnixDate(std::int64_t timestamp);
}
