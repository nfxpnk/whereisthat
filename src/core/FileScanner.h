#pragma once
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
    using ProgressCallback = std::function<void(const Progress&)>;
    bool ScanFolder(const std::wstring& rootPath, std::int64_t catalogId, wit::storage::Database& db, const ProgressCallback& onProgress);
};
}
