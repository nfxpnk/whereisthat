#pragma once
#include <cstdint>
#include <functional>
#include <stop_token>
#include <string>
#include "wit_database/Database.h"

namespace wit::infra {
struct ScanProfile;
}

namespace wit::core {
class FileScanner {
public:
    struct Options {
        bool enableScanFileDelay{};
    };
    struct Progress {
        std::uint64_t scannedFiles{};
        std::uint64_t scannedFolders{};
        std::wstring currentPath;
    };
    struct Result {
        std::uint64_t totalFiles{};
        std::uint64_t totalFolders{};
        std::uint64_t scannedArchives{};
        std::uint64_t archiveFiles{};
        std::uint64_t archiveFolders{};
        std::int64_t elapsedMilliseconds{};
    };
    using ProgressCallback = std::function<void(const Progress&)>;
    using DiagnosticCallback = std::function<void(const std::wstring&)>;
    explicit FileScanner(Options options = {});
    [[nodiscard]] bool CountFiles(const std::wstring& rootPath, std::uint64_t& totalFiles, std::stop_token stopToken = {}) const;
    [[nodiscard]] bool ScanFolder(const std::wstring& rootPath, std::int64_t diskId, wit::storage::Database& db,
        const ProgressCallback& onProgress, Result& result, bool calculateCrc, bool manageTransaction = true,
        std::stop_token stopToken = {}, bool browseArchives = false,
        const DiagnosticCallback& onDiagnostic = {}, wit::infra::ScanProfile* profile = nullptr);

private:
    Options options_;
};
}

