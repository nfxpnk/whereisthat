#pragma once
#include <filesystem>
#include <string>

namespace wit::platform {
std::wstring EnsureLongPathPrefix(const std::wstring& path);
std::wstring Join(const std::wstring& parent, const std::wstring& child);
std::wstring ParentDirectory(const std::wstring& fullPath);
std::wstring FileExtension(const std::wstring& fileName);
std::wstring DisplayNameForPath(const std::filesystem::path& path);
}
