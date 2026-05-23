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

}

std::wstring SettingsFilePath() {
    const auto directory = ExecutableDirectory();
    return directory.empty() ? L"settings.ini" : directory + L"\\settings.ini";
}

AppSettings LoadAppSettings() {
    AppSettings settings{};
    settings.showStatusBar =
        GetPrivateProfileIntW(L"General", L"ShowStatusBar", 1, SettingsFilePath().c_str()) != 0;
    return settings;
}

bool SaveAppSettings(const AppSettings& settings) {
    return WritePrivateProfileStringW(L"General", L"ShowStatusBar",
        settings.showStatusBar ? L"1" : L"0", SettingsFilePath().c_str()) != FALSE;
}

}
