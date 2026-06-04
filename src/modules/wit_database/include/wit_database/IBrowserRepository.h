#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "wit_types/BrowserLocation.h"
#include "wit_types/Disk.h"
#include "wit_types/FileEntry.h"

namespace wit::storage {

class IBrowserRepository {
public:
    virtual ~IBrowserRepository() = default;

    virtual int GetBrowserItemCount(const wit::core::BrowserLocation& location) = 0;
    virtual int GetBrowserRootItemCount(const wit::core::BrowserLocation& location) = 0;
    virtual std::vector<wit::core::BrowserItem> GetBrowserRootItemsPage(
        const wit::core::BrowserLocation& location,
        int offset,
        int limit) = 0;
    virtual std::vector<wit::core::FileEntry> GetBrowserItemsPage(
        const wit::core::BrowserLocation& location,
        int offset,
        int limit) = 0;
    virtual bool HasChildFolders(std::int64_t sourceId, const std::wstring& parentPath) = 0;
    virtual std::vector<wit::core::FileEntry> GetChildFolders(
        std::int64_t sourceId,
        const std::wstring& parentPath) = 0;
};

}
