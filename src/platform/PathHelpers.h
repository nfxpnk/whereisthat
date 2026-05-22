#pragma once
#include <string>

namespace wit::platform {
std::wstring EnsureLongPathPrefix(const std::wstring& path);
std::wstring ParentDirectory(const std::wstring& fullPath);
std::wstring FileExtension(const std::wstring& fileName);
}
