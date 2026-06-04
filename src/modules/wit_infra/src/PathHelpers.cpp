#include <wit_infra/PathHelpers.h>

namespace wit::platform {
std::wstring EnsureLongPathPrefix(const std::wstring& path) {
    if (path.rfind(L"\\\\?\\", 0) == 0) return path;
    if (path.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + path.substr(2);
    return L"\\\\?\\" + path;
}
std::wstring Join(const std::wstring& parent, const std::wstring& child) {
    if (parent.empty()) return child;
    const auto last = parent.back();
    return (last == L'\\' || last == L'/') ? parent + child : parent + L"\\" + child;
}
std::wstring ParentDirectory(const std::wstring& fullPath) {
    auto pos = fullPath.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"" : fullPath.substr(0, pos);
}
std::wstring FileExtension(const std::wstring& fileName) {
    auto pos = fileName.find_last_of(L'.');
    return pos == std::wstring::npos || pos + 1 >= fileName.size() ? L"" : fileName.substr(pos + 1);
}
std::wstring DisplayNameForPath(const std::filesystem::path& path) {
    if (path.has_root_name() && path == path.root_path()) return path.wstring();
    auto name = path.filename();
    if (name.empty()) name = path.parent_path().filename();
    return name.empty() ? path.wstring() : name.wstring();
}
}
