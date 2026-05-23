#include "AppSettings.h"

#include <Windows.h>
#include <vector>
#include "PathHelpers.h"

namespace wit::platform {
namespace {

std::wstring ExecutableDirectory() {
    DWORD capacity = MAX_PATH;
    for (;;) {
        std::vector<wchar_t> buffer(capacity);
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), capacity);
        if (length == 0) return {};
        if (length < capacity - 1) {
            return ParentDirectory(std::wstring(buffer.data(), length));
        }
        capacity *= 2;
    }
}

std::wstring ReadProfileString(const wchar_t* section, const wchar_t* key, const std::wstring& filePath) {
    DWORD capacity = MAX_PATH;
    for (;;) {
        std::vector<wchar_t> buffer(capacity);
        const DWORD length = GetPrivateProfileStringW(section, key, L"", buffer.data(), capacity, filePath.c_str());
        if (length < capacity - 1) return std::wstring(buffer.data(), length);
        capacity *= 2;
    }
}

}

std::wstring SettingsFilePath() {
    const auto directory = ExecutableDirectory();
    return directory.empty() ? L"settings.ini" : directory + L"\\settings.ini";
}

AppSettings LoadAppSettings() {
    AppSettings settings{};
    const auto path = SettingsFilePath();
    settings.showStatusBar =
        GetPrivateProfileIntW(L"General", L"ShowStatusBar", 1, path.c_str()) != 0;
    settings.lastCatalogPath = ReadProfileString(L"General", L"LastCatalogPath", path);
    return settings;
}

bool SaveAppSettings(const AppSettings& settings) {
    const auto path = SettingsFilePath();
    return WritePrivateProfileStringW(L"General", L"ShowStatusBar",
        settings.showStatusBar ? L"1" : L"0", path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"General", L"LastCatalogPath",
            settings.lastCatalogPath.c_str(), path.c_str()) != FALSE;
}

}
