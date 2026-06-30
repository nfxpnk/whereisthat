#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "wit_database/IBrowserRepository.h"
#include "wit_types/BrowserLocation.h"
#include "wit_types/Disk.h"
#include "wit_types/FileEntry.h"

struct sqlite3;

namespace wit::storage {

class SqliteBrowserRepository : public IBrowserRepository {
public:
    explicit SqliteBrowserRepository(sqlite3* db);

    void SetDatabase(sqlite3* db);

    int GetBrowserItemCount(const wit::core::BrowserLocation& location) override;

    int GetBrowserRootItemCount(const wit::core::BrowserLocation& location) override;

    std::vector<wit::core::BrowserItem> GetBrowserRootItemsPage(
        const wit::core::BrowserLocation& location,
        int offset,
        int limit,
        wit::core::BrowserRootSort sort = {}) override;

    std::vector<wit::core::FileEntry> GetBrowserItemsPage(
        const wit::core::BrowserLocation& location,
        int offset,
        int limit,
        wit::core::FileSort sort) override;

    bool HasChildFolders(
        std::int64_t sourceId,
        const std::wstring& parentPath) override;

    std::vector<wit::core::FileEntry> GetChildFolders(
        std::int64_t sourceId,
        const std::wstring& parentPath) override;

    std::wstring LastErrorMessage() const override;

private:
    void ClearLastError();
    void SetLastError(const wchar_t* fallback);

    sqlite3* db_{};
    mutable std::mutex errorMutex_;
    std::wstring lastError_;
};

}
