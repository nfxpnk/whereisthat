#pragma once

#include <map>
#include <string>
#include <vector>

namespace wit::platform {

struct OpenCatalogSettings {
    std::vector<std::wstring> paths;
    int lastActiveCatalog{};
    bool hasMultiCatalogSettings{};
};

struct AppSettings {
    bool showStatusBar{true};
    bool showToolbar{true};
    bool doNotShowAlphaWarning{false};
    bool enableScanFileDelay{false};
    int mainSplitterPosition{360};
    std::wstring dateTimeFormat;
    std::wstring lastCatalogPath;
    std::vector<std::wstring> openCatalogPaths;
    int lastActiveCatalog{};
    bool hasMultiCatalogSettings{};
    std::vector<std::wstring> recentCatalogPaths;
    std::map<std::wstring, int> fileListColumnWidths;
};

std::wstring SettingsFilePath();
OpenCatalogSettings ReadOpenCatalogSettings();
[[nodiscard]] bool WriteOpenCatalogSettings(const std::vector<std::wstring>& paths, int lastActiveCatalog);
AppSettings LoadAppSettings();
[[nodiscard]] bool SaveAppSettings(const AppSettings& settings);
void RememberRecentCatalog(AppSettings& settings, const std::wstring& path);

}
