#pragma once
#include <Windows.h>
#include <string>

namespace wit::platform {
std::string ToUtf8(const std::wstring& value);
std::wstring ToUtf16(const std::string& value);
std::wstring FileTimeToIso8601(const FILETIME& fileTime);
std::wstring NowIso8601();
}
