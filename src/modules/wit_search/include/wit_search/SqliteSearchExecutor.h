#pragma once
#include <chrono>
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

    PreparedSearchResult PrepareByName(
        const std::wstring& nameTerm, int limit, wit::core::FileSort sort = {},
        bool caseSensitive = false) override;
    PreparedSearchResult PrepareAdvanced(
        const AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort = {}) override;
    int CountByName(const std::wstring& nameTerm, bool caseSensitive = false) override;
    std::vector<wit::core::FileEntry> PageByName(
        const std::wstring& nameTerm, int offset, int limit, wit::core::FileSort sort = {},
        bool caseSensitive = false) override;
    int CountAdvanced(const AdvancedSearchExpression& expression) override;
    std::vector<wit::core::FileEntry> PageAdvanced(
        const AdvancedSearchExpression& expression,
        int offset,
        int limit,
        wit::core::FileSort sort = {}) override;

    void CancelPending() override;
    std::wstring LastErrorMessage() const override;
    SearchMetrics LastMetrics() const override;
private:
    void CloseSearchDatabase();
    sqlite3* ActiveDatabase() const;
    std::string DatabaseGenerationKey() const;
    void SetLastError(sqlite3* db, const wchar_t* fallback);
    void RecordCacheBuildStarted();
    void RecordCacheBuildFinished(std::chrono::steady_clock::time_point startedAt, bool success, bool cancelled);
    void RecordRowsMaterialized(std::uint64_t rows);
    void RecordFirstPageReady(std::chrono::steady_clock::time_point startedAt, std::uint64_t rows);
    int PageCacheCountLocked(sqlite3* db);
    std::vector<wit::core::FileEntry> PageByNameLocked(
        const std::wstring& nameTerm, int offset, int limit, wit::core::FileSort sort,
        bool caseSensitive);
    std::vector<wit::core::FileEntry> PageAdvancedLocked(
        const AdvancedSearchExpression& expression, int offset, int limit, wit::core::FileSort sort);

    sqlite3* sourceDb_{};
    sqlite3* searchDb_{};
    bool ownsSearchDb_{};
    mutable std::mutex operationMutex_;
    mutable std::mutex errorMutex_;
    mutable std::mutex metricsMutex_;
    std::wstring lastError_;
    SearchMetrics metrics_;
    std::string pageCacheKey_;
    std::string pageCacheQueryKey_;
    bool pageCacheValid_{};
    bool pageCachePinned_{};
};
}
