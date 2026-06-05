#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

namespace wit::platform {
std::string ToUtf8(const std::wstring& value);
std::wstring ToUtf16(const std::string& value);
std::int64_t FileTimeToUnixSeconds(const FILETIME& fileTime);
std::int64_t NowUnixSeconds();
void SetDateTimeFormatOverride(const std::wstring& pattern);
std::wstring DateTimeFormatOverride();
bool IsValidDateTimeFormat(const std::wstring& pattern);
std::wstring FormatDateTimeSample(const std::wstring& pattern);
std::wstring FormatUnixTimestamp(std::int64_t timestamp);
void FormatUnixTimestampToBuffer(std::int64_t timestamp, wchar_t* buffer, std::size_t bufferSize);
std::wstring FormatUnixDate(std::int64_t timestamp);
}
