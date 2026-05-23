#pragma once

#include <string>

namespace wit::platform {

struct AppSettings {
    bool showStatusBar{true};
};

std::wstring SettingsFilePath();
AppSettings LoadAppSettings();
bool SaveAppSettings(const AppSettings& settings);

}
