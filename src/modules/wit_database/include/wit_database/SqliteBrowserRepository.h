#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "wit_types/BrowserLocation.h"
#include "wit_types/Disk.h"
#include "wit_types/FileEntry.h"

struct sqlite3;

namespace wit::storage {

class SqliteBrowserRepository {
public:
    explicit SqliteBrowserRepository(sqlite3* db);

    int GetBrowserItemCount(const wit::core::BrowserLocation& location);

    int GetBrowserRootItemCount(const wit::core::BrowserLocation& location);

    std::vector<wit::core::BrowserItem> GetBrowserRootItemsPage(
        const wit::core::BrowserLocation& location,
        int offset,
        int limit);

    std::vector<wit::core::FileEntry> GetBrowserItemsPage(
        const wit::core::BrowserLocation& location,
        int offset,
        int limit);

    bool HasChildFolders(
        std::int64_t sourceId,
        const std::wstring& parentPath);

    std::vector<wit::core::FileEntry> GetChildFolders(
        std::int64_t sourceId,
        const std::wstring& parentPath);

private:
    sqlite3* db_{};
};

}
