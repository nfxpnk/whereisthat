#include "PathHelpers.h"

namespace wit::platform {
std::wstring EnsureLongPathPrefix(const std::wstring& path) {
    if (path.rfind(L"\\\\?\\", 0) == 0) return path;
    if (path.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + path.substr(2);
    return L"\\\\?\\" + path;
}
std::wstring ParentDirectory(const std::wstring& fullPath) {
    auto pos = fullPath.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"" : fullPath.substr(0, pos);
}
std::wstring FileExtension(const std::wstring& fileName) {
    auto pos = fileName.find_last_of(L'.');
    return pos == std::wstring::npos || pos + 1 >= fileName.size() ? L"" : fileName.substr(pos + 1);
}
}
