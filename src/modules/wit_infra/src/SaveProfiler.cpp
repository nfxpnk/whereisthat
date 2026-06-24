#include "wit_infra/SaveProfiler.h"

#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace wit::infra {
namespace {

thread_local SaveProfile* g_currentProfile{};
std::atomic<std::uint64_t> g_nextProfileId{1};

std::wstring ExecutableDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return std::filesystem::current_path().wstring();
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path().wstring();
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::string EscapeJsonString(const std::wstring& value) {
    std::ostringstream out;
    out << '"';
    for (const wchar_t ch : value) {
        switch (ch) {
        case L'"': out << "\\\""; break;
        case L'\\': out << "\\\\"; break;
        case L'\b': out << "\\b"; break;
        case L'\f': out << "\\f"; break;
        case L'\n': out << "\\n"; break;
        case L'\r': out << "\\r"; break;
        case L'\t': out << "\\t"; break;
        default:
            if (ch >= 0 && ch < 0x20) {
                out << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec << std::nouppercase;
            } else if (ch <= 0x7F) {
                out << static_cast<char>(ch);
            } else if (ch <= 0x7FF) {
                out << static_cast<char>(0xC0 | ((ch >> 6) & 0x1F));
                out << static_cast<char>(0x80 | (ch & 0x3F));
            } else if (ch >= 0xD800 && ch <= 0xDFFF) {
                out << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec << std::nouppercase;
            } else {
                out << static_cast<char>(0xE0 | ((ch >> 12) & 0x0F));
                out << static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                out << static_cast<char>(0x80 | (ch & 0x3F));
            }
            break;
        }
    }
    out << '"';
    return out.str();
}

std::string EscapeJsonString(const std::string& value) {
    return EscapeJsonString(std::wstring(value.begin(), value.end()));
}

std::filesystem::path ProfilePath(const SaveProfile& profile) {
    const auto directory = std::filesystem::path(ExecutableDirectory()) / L"save-profiles";
    std::filesystem::create_directories(directory);

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::wostringstream name;
    name << L"save-profile-"
        << std::setw(4) << std::setfill(L'0') << time.wYear
        << std::setw(2) << time.wMonth
        << std::setw(2) << time.wDay
        << L"-"
        << std::setw(2) << time.wHour
        << std::setw(2) << time.wMinute
        << std::setw(2) << time.wSecond
        << L"-" << GetCurrentProcessId()
        << L"-" << profile.profileId
        << L".json";
    return directory / name.str();
}

}

ScopedSaveTimer::ScopedSaveTimer(std::uint64_t& accumulator) noexcept
    : accumulator_(&accumulator), start_(std::chrono::steady_clock::now()) {
}

ScopedSaveTimer::ScopedSaveTimer(ScopedSaveTimer&& other) noexcept
    : accumulator_(std::exchange(other.accumulator_, nullptr)), start_(other.start_) {
}

ScopedSaveTimer::~ScopedSaveTimer() noexcept {
    if (!accumulator_) return;
    const auto elapsed = std::chrono::steady_clock::now() - start_;
    *accumulator_ += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

SaveProfileScope::SaveProfileScope(SaveProfile& profile) noexcept
    : previous_(g_currentProfile) {
    g_currentProfile = &profile;
}

SaveProfileScope::~SaveProfileScope() noexcept {
    g_currentProfile = previous_;
}

SaveProfile* CurrentSaveProfile() noexcept {
    return g_currentProfile;
}

std::uint64_t NextSaveProfileId() noexcept {
    return g_nextProfileId.fetch_add(1, std::memory_order_relaxed);
}

bool WriteSaveProfileJson(const SaveProfile& profile) {
    try {
        std::ofstream out(ProfilePath(profile), std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << "{\n"
            << "  \"schemaVersion\": 1,\n"
            << "  \"profileId\": " << profile.profileId << ",\n"
            << "  \"catalogId\": " << profile.catalogId << ",\n"
            << "  \"catalogPath\": " << EscapeJsonString(profile.catalogPath) << ",\n"
            << "  \"operation\": " << EscapeJsonString(profile.operation) << ",\n"
            << "  \"result\": " << EscapeJsonString(profile.result) << ",\n"
            << "  \"timingsNs\": {\n"
            << "    \"total\": " << profile.timingsNs.total << ",\n"
            << "    \"commandHandle\": " << profile.timingsNs.commandHandle << ",\n"
            << "    \"applyControllerResult\": " << profile.timingsNs.applyControllerResult << ",\n"
            << "    \"requestSave\": " << profile.timingsNs.requestSave << ",\n"
            << "    \"saveCatalog\": " << profile.timingsNs.saveCatalog << ",\n"
            << "    \"createDiskGroup\": " << profile.timingsNs.createDiskGroup << ",\n"
            << "    \"moveDiskToGroup\": " << profile.timingsNs.moveDiskToGroup << ",\n"
            << "    \"moveDiskGroupToGroup\": " << profile.timingsNs.moveDiskGroupToGroup << ",\n"
            << "    \"acceptPending\": " << profile.timingsNs.acceptPending << ",\n"
            << "    \"savePending\": " << profile.timingsNs.savePending << ",\n"
            << "    \"saveCatalogDataFrom\": " << profile.timingsNs.saveCatalogDataFrom << ",\n"
            << "    \"createWorkingCopy\": " << profile.timingsNs.createWorkingCopy << ",\n"
            << "    \"backupCreateWorkingCopy\": " << profile.timingsNs.backupCreateWorkingCopy << ",\n"
            << "    \"backupSavePendingToTemp\": " << profile.timingsNs.backupSavePendingToTemp << ",\n"
            << "    \"verifyCatalog\": " << profile.timingsNs.verifyCatalog << ",\n"
            << "    \"integrityCheck\": " << profile.timingsNs.integrityCheck << ",\n"
            << "    \"prepareTempSingleFileCatalog\": " << profile.timingsNs.prepareTempSingleFileCatalog << ",\n"
            << "    \"prepareActiveSingleFileCatalog\": " << profile.timingsNs.prepareActiveSingleFileCatalog << ",\n"
            << "    \"walCheckpointTruncate\": " << profile.timingsNs.walCheckpointTruncate << ",\n"
            << "    \"journalModeDelete\": " << profile.timingsNs.journalModeDelete << ",\n"
            << "    \"journalModeWal\": " << profile.timingsNs.journalModeWal << ",\n"
            << "    \"replaceCatalogFile\": " << profile.timingsNs.replaceCatalogFile << ",\n"
            << "    \"closeActiveCatalog\": " << profile.timingsNs.closeActiveCatalog << ",\n"
            << "    \"reopenReplacement\": " << profile.timingsNs.reopenReplacement << ",\n"
            << "    \"openInternal\": " << profile.timingsNs.openInternal << ",\n"
            << "    \"browserRefreshCatalog\": " << profile.timingsNs.browserRefreshCatalog << ",\n"
            << "    \"treeRefreshCatalog\": " << profile.timingsNs.treeRefreshCatalog << ",\n"
            << "    \"treePopulateRoot\": " << profile.timingsNs.treePopulateRoot << "\n"
            << "  }\n"
            << "}\n";
        return out.good();
    } catch (...) {
        return false;
    }
}

}
