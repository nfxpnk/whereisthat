#pragma once
#include "wit_scanner/FileScanner.h"

namespace wit::core {
class IFileScanner {
public:
    virtual ~IFileScanner() = default;
    [[nodiscard]] virtual bool CountFiles(const std::wstring& rootPath, std::uint64_t& totalFiles, std::stop_token stopToken = {}) const = 0;
};
}

