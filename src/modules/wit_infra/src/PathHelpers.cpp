#include <wit_infra/PathHelpers.h>

#include <algorithm>

namespace wit::platform {
std::wstring EnsureLongPathPrefix(const std::wstring& path) {
#ifdef _WIN32
    if (path.rfind(L"\\\\?\\", 0) == 0) return path;
    if (path.rfind(L"\\\\.\\", 0) == 0) return path;
    if (path.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + path.substr(2);
    if (path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) {
        auto prefixed = L"\\\\?\\" + path;
        std::ranges::replace(prefixed, L'/', L'\\');
        return prefixed;
    }
#endif
    return path;
}
std::wstring Join(const std::wstring& parent, const std::wstring& child) {
    return (std::filesystem::path(parent) / std::filesystem::path(child)).wstring();
}
std::wstring ParentDirectory(const std::wstring& fullPath) {
    return std::filesystem::path(fullPath).parent_path().wstring();
}
std::wstring FileExtension(const std::wstring& fileName) {
    auto extension = std::filesystem::path(fileName).extension().wstring();
    if (!extension.empty() && extension.front() == L'.') extension.erase(extension.begin());
    return extension;
}
std::wstring DisplayNameForPath(const std::filesystem::path& path) {
    if (path == path.root_path()) return path.wstring();
    auto name = path.filename();
    if (name.empty()) name = path.parent_path().filename();
    return name.empty() ? path.wstring() : name.wstring();
}
}
