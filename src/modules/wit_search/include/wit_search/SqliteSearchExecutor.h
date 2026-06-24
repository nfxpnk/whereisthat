#pragma once
#include <mutex>
#include <string>
#include <vector>
#include <wit_search/ISearchRepository.h>
#include <wit_types/FileEntry.h>

struct sqlite3;

namespace wit::search {
class SqliteSearchExecutor : public ISearchRepository {
public:
    explicit SqliteSearchExecutor(sqlite3* db);
    ~SqliteSearchExecutor() override;

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

    void CancelPending() override;
    std::wstring LastErrorMessage() const override;
private:
    void CloseSearchDatabase();
    sqlite3* ActiveDatabase() const;
    std::string DatabaseGenerationKey() const;
    void SetLastError(sqlite3* db, const wchar_t* fallback);

    sqlite3* sourceDb_{};
    sqlite3* searchDb_{};
    bool ownsSearchDb_{};
    mutable std::mutex operationMutex_;
    mutable std::mutex errorMutex_;
    std::wstring lastError_;
    std::string pageCacheKey_;
    bool pageCacheValid_{};
};
}
