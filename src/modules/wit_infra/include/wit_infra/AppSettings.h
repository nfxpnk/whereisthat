#pragma once

#include <string>
#include <vector>

namespace wit::platform {

struct AppSettings {
    bool showStatusBar{true};
    int mainSplitterPosition{360};
    std::wstring lastCatalogPath;
    std::vector<std::wstring> recentCatalogPaths;
};

std::wstring SettingsFilePath();
AppSettings LoadAppSettings();
bool SaveAppSettings(const AppSettings& settings);
void RememberRecentCatalog(AppSettings& settings, const std::wstring& path);

}
