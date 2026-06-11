#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace wit::infra {

struct ScanProfileCounts {
    std::uint64_t files{};
    std::uint64_t folders{};
    std::uint64_t archiveProbeAttempts{};
    std::uint64_t archivesRecognized{};
    std::uint64_t archiveFiles{};
    std::uint64_t archiveFolders{};
    std::uint64_t crcFiles{};
    std::uint64_t dbInsertFileCalls{};
    std::uint64_t dbInsertFolderCalls{};
    std::uint64_t dbUpdateFolderContentSizeCalls{};
    std::uint64_t progressReports{};
};

struct ScanProfileBytes {
    std::uint64_t fileBytesSeen{};
    std::uint64_t crcBytesRead{};
    std::uint64_t archiveBytesRead{};
};

struct ScanProfileTimings {
    std::uint64_t total{};
    std::uint64_t workingCopyCreate{};
    std::uint64_t countFiles{};
    std::uint64_t beginTransaction{};
    std::uint64_t populateVolumeMetadata{};
    std::uint64_t findExistingDisk{};
    std::uint64_t deleteOldDiskContent{};
    std::uint64_t addDisk{};
    std::uint64_t scanFolder{};
    std::uint64_t directoryEnumeration{};
    std::uint64_t metadataBuild{};
    std::uint64_t dbInsertFile{};
    std::uint64_t dbInsertFolder{};
    std::uint64_t dbUpdateFolderContentSize{};
    std::uint64_t archiveProbe{};
    std::uint64_t archiveRead{};
    std::uint64_t crcReadAndCompute{};
    std::uint64_t progressCallbacks{};
    std::uint64_t updateDisk{};
    std::uint64_t updateDiskScanStatistics{};
    std::uint64_t commit{};
    std::uint64_t rollback{};
    std::uint64_t saveCatalog{};
};

struct ScanProfileSqlite {
    std::uint64_t prepareInsertFile{};
    std::uint64_t prepareInsertFolder{};
    std::uint64_t prepareUpdateFolderContentSize{};
    std::uint64_t stepInsertFile{};
    std::uint64_t stepInsertFolder{};
    std::uint64_t stepUpdateFolderContentSize{};
    std::uint64_t resetInsertFile{};
    std::uint64_t resetInsertFolder{};
    std::uint64_t resetUpdateFolderContentSize{};
    std::uint64_t finalizeInsertFile{};
    std::uint64_t finalizeInsertFolder{};
    std::uint64_t finalizeUpdateFolderContentSize{};
};

struct ScanProfileOptions {
    bool calculateCrc{};
    bool browseArchives{};
    bool countFilesBeforeScan{true};
    bool profilingEnabled{true};
};

struct ScanProfile {
    std::uint64_t scanId{};
    std::wstring root;
    std::string result{"failed"};
    ScanProfileOptions options;
    ScanProfileCounts counts;
    ScanProfileBytes bytes;
    ScanProfileTimings timingsNs;
    ScanProfileSqlite sqlite;
};

class ScopedScanTimer {
public:
    explicit ScopedScanTimer(std::uint64_t& accumulator) noexcept;
    ~ScopedScanTimer() noexcept;

    ScopedScanTimer(const ScopedScanTimer&) = delete;
    ScopedScanTimer& operator=(const ScopedScanTimer&) = delete;
    ScopedScanTimer(ScopedScanTimer&& other) noexcept;
    ScopedScanTimer& operator=(ScopedScanTimer&&) = delete;

private:
    std::uint64_t* accumulator_;
    std::chrono::steady_clock::time_point start_;
};

[[nodiscard]] bool WriteScanProfileJson(const ScanProfile& profile);

}
