#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include "../storage/Database.h"

namespace wit::core {
class FileScanner {
public:
    struct Progress {
        std::uint64_t scannedFiles{};
        std::uint64_t scannedFolders{};
        std::wstring currentPath;
    };
    struct Result {
        std::uint64_t totalFiles{};
        std::uint64_t totalFolders{};
        std::int64_t elapsedMilliseconds{};
    };
    using ProgressCallback = std::function<void(const Progress&)>;
    bool ScanFolder(const std::wstring& rootPath, std::int64_t diskId, wit::storage::Database& db,
        const ProgressCallback& onProgress, Result& result, bool calculateCrc, bool manageTransaction = true);
};
}
