#include <wit_infra/AppSettings.h>

#include <Windows.h>
#include <cwchar>
#include <format>
#include <string_view>
#include <vector>
#include <wit_infra/PathHelpers.h>
#include <wit_infra/Win32Helpers.h>

namespace wit::platform {
namespace {
constexpr std::size_t kMaximumRecentCatalogs = 10;
constexpr int kDefaultMainSplitterPosition = 360;
constexpr const wchar_t* kFileListColumnWidthsSection = L"FileListColumnWidths";

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

std::vector<std::wstring> ReadProfileSection(const wchar_t* section, const std::wstring& filePath) {
    DWORD capacity = 1024;
    for (;;) {
        std::vector<wchar_t> buffer(capacity);
        const DWORD length = GetPrivateProfileSectionW(section, buffer.data(), capacity, filePath.c_str());
        if (length < capacity - 2) {
            std::vector<std::wstring> entries;
            const wchar_t* cursor = buffer.data();
            while (*cursor != L'\0') {
                const std::wstring entry(cursor);
                entries.push_back(entry);
                cursor += entry.size() + 1;
            }
            return entries;
        }
        capacity *= 2;
    }
}

bool TryParseInt(std::wstring_view text, int& value) {
    if (text.empty()) return false;
    std::wstring nullTerminated(text);
    wchar_t* end{};
    const long parsed = std::wcstol(nullTerminated.c_str(), &end, 10);
    if (end == nullTerminated.c_str() || *end != L'\0') return false;
    value = static_cast<int>(parsed);
    return static_cast<long>(value) == parsed;
}

void LoadFileListColumnWidths(AppSettings& settings, const std::wstring& path) {
    for (const auto& entry : ReadProfileSection(kFileListColumnWidthsSection, path)) {
        const auto separator = entry.find(L'=');
        if (separator == std::wstring::npos || separator == 0) continue;
        int width{};
        if (!TryParseInt(std::wstring_view(entry).substr(separator + 1), width)) continue;
        settings.fileListColumnWidths.emplace(entry.substr(0, separator), width);
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
    settings.showToolbar =
        GetPrivateProfileIntW(L"General", L"ShowToolbar", 1, path.c_str()) != 0;
    settings.doNotShowAlphaWarning =
        GetPrivateProfileIntW(L"General", L"DoNotShowAlphaWarning", 0, path.c_str()) != 0;
    settings.enableScanFileDelay =
        GetPrivateProfileIntW(L"General", L"EnableScanFileDelay", 0, path.c_str()) != 0;
    settings.mainSplitterPosition = GetPrivateProfileIntW(
        L"General", L"MainSplitterPosition", kDefaultMainSplitterPosition, path.c_str());
    settings.dateTimeFormat = ReadProfileString(L"General", L"DateTimeFormat", path);
    if (!wit::platform::IsValidDateTimeFormat(settings.dateTimeFormat)) {
        settings.dateTimeFormat.clear();
    }
    settings.lastCatalogPath = ReadProfileString(L"General", L"LastCatalogPath", path);
    for (std::size_t index = kMaximumRecentCatalogs; index > 0; --index) {
        const auto key = std::format(L"Path{}", index);
        auto recentPath = ReadProfileString(L"RecentCatalogs", key.c_str(), path);
        if (!recentPath.empty()) RememberRecentCatalog(settings, recentPath);
    }
    if (!settings.lastCatalogPath.empty()) RememberRecentCatalog(settings, settings.lastCatalogPath);
    LoadFileListColumnWidths(settings, path);
    wit::platform::SetDateTimeFormatOverride(settings.dateTimeFormat);
    return settings;
}

bool SaveAppSettings(const AppSettings& settings) {
    const auto path = SettingsFilePath();
    const auto splitterPosition = std::format(L"{}", settings.mainSplitterPosition);
    bool success = WritePrivateProfileStringW(L"General", L"ShowStatusBar",
        settings.showStatusBar ? L"1" : L"0", path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"General", L"ShowToolbar",
            settings.showToolbar ? L"1" : L"0", path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"General", L"DoNotShowAlphaWarning",
            settings.doNotShowAlphaWarning ? L"1" : L"0", path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"General", L"EnableScanFileDelay",
            settings.enableScanFileDelay ? L"1" : L"0", path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"General", L"MainSplitterPosition",
            splitterPosition.c_str(), path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"General", L"DateTimeFormat",
            settings.dateTimeFormat.c_str(), path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"General", L"LastCatalogPath",
            settings.lastCatalogPath.c_str(), path.c_str()) != FALSE;
    for (std::size_t index = 0; index < kMaximumRecentCatalogs; ++index) {
        const auto key = std::format(L"Path{}", index + 1);
        const wchar_t* value = index < settings.recentCatalogPaths.size()
            ? settings.recentCatalogPaths[index].c_str() : nullptr;
        success = WritePrivateProfileStringW(L"RecentCatalogs", key.c_str(), value, path.c_str()) != FALSE &&
            success;
    }
    for (const auto& [key, value] : settings.fileListColumnWidths) {
        const auto formatted = std::format(L"{}", value);
        success = WritePrivateProfileStringW(kFileListColumnWidthsSection, key.c_str(), formatted.c_str(),
            path.c_str()) != FALSE && success;
    }
    if (success) wit::platform::SetDateTimeFormatOverride(settings.dateTimeFormat);
    return success;
}

void RememberRecentCatalog(AppSettings& settings, const std::wstring& path) {
    if (path.empty()) return;
    std::erase_if(settings.recentCatalogPaths, [&path](const std::wstring& existing) {
        return CompareStringOrdinal(existing.c_str(), -1, path.c_str(), -1, TRUE) == CSTR_EQUAL;
    });
    settings.recentCatalogPaths.insert(settings.recentCatalogPaths.begin(), path);
    if (settings.recentCatalogPaths.size() > kMaximumRecentCatalogs) {
        settings.recentCatalogPaths.resize(kMaximumRecentCatalogs);
    }
}

}
