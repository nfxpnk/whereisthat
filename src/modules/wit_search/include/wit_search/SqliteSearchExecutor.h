#pragma once
#include <string>
#include <vector>
#include <wit_search/ISearchRepository.h>
#include <wit_types/FileEntry.h>

struct sqlite3;

namespace wit::search {
class SqliteSearchExecutor : public ISearchRepository {
public:
    explicit SqliteSearchExecutor(sqlite3* db);

    void SetDatabase(sqlite3* db);

    int CountByName(const std::wstring& nameTerm) override;
    std::vector<wit::core::FileEntry> PageByName(
        const std::wstring& nameTerm, int offset, int limit, wit::core::FileSort sort = {}) override;
    int CountAdvanced(const AdvancedSearchExpression& expression) override;
    std::vector<wit::core::FileEntry> PageAdvanced(
        const AdvancedSearchExpression& expression,
        int offset,
        int limit,
        wit::core::FileSort sort = {}) override;

private:
    sqlite3* db_{};
    std::string pageCacheKey_;
    bool pageCacheValid_{};
};
}
