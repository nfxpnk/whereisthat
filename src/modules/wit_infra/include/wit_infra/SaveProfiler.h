#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace wit::infra {

struct SaveProfileTimings {
    std::uint64_t total{};
    std::uint64_t commandHandle{};
    std::uint64_t applyControllerResult{};
    std::uint64_t requestSave{};
    std::uint64_t saveCatalog{};
    std::uint64_t createDiskGroup{};
    std::uint64_t moveDiskToGroup{};
    std::uint64_t moveDiskGroupToGroup{};
    std::uint64_t acceptPending{};
    std::uint64_t savePending{};
    std::uint64_t saveCatalogDataFrom{};
    std::uint64_t createWorkingCopy{};
    std::uint64_t backupCreateWorkingCopy{};
    std::uint64_t backupSavePendingToTemp{};
    std::uint64_t verifyCatalog{};
    std::uint64_t integrityCheck{};
    std::uint64_t prepareTempSingleFileCatalog{};
    std::uint64_t prepareActiveSingleFileCatalog{};
    std::uint64_t walCheckpointTruncate{};
    std::uint64_t journalModeDelete{};
    std::uint64_t journalModeWal{};
    std::uint64_t replaceCatalogFile{};
    std::uint64_t closeActiveCatalog{};
    std::uint64_t reopenReplacement{};
    std::uint64_t openInternal{};
    std::uint64_t browserRefreshCatalog{};
    std::uint64_t treeRefreshCatalog{};
    std::uint64_t treePopulateRoot{};
};

struct SaveProfile {
    std::uint64_t profileId{};
    std::uint64_t catalogId{};
    std::wstring catalogPath;
    std::wstring operation;
    std::string result{"failed"};
    SaveProfileTimings timingsNs;
};

class ScopedSaveTimer {
public:
    explicit ScopedSaveTimer(std::uint64_t& accumulator) noexcept;
    ~ScopedSaveTimer() noexcept;

    ScopedSaveTimer(const ScopedSaveTimer&) = delete;
    ScopedSaveTimer& operator=(const ScopedSaveTimer&) = delete;
    ScopedSaveTimer(ScopedSaveTimer&& other) noexcept;
    ScopedSaveTimer& operator=(ScopedSaveTimer&&) = delete;

private:
    std::uint64_t* accumulator_;
    std::chrono::steady_clock::time_point start_;
};

class SaveProfileScope {
public:
    explicit SaveProfileScope(SaveProfile& profile) noexcept;
    ~SaveProfileScope() noexcept;

    SaveProfileScope(const SaveProfileScope&) = delete;
    SaveProfileScope& operator=(const SaveProfileScope&) = delete;

private:
    SaveProfile* previous_;
};

[[nodiscard]] SaveProfile* CurrentSaveProfile() noexcept;
[[nodiscard]] std::uint64_t NextSaveProfileId() noexcept;
[[nodiscard]] bool WriteSaveProfileJson(const SaveProfile& profile);

}
