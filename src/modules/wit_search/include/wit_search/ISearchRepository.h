#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "wit_search/AdvancedSearchParser.h"
#include "wit_types/FileEntry.h"
#include "wit_types/FileSort.h"

namespace wit::search {

struct PreparedSearchResult {
    int total{};
    std::vector<wit::core::FileEntry> entries;
};

struct SearchMetrics {
    std::uint64_t cacheBuildsStarted{};
    std::uint64_t cacheBuildsCompleted{};
    std::uint64_t cacheBuildsCancelled{};
    std::uint64_t cacheBuildDurationNs{};
    std::uint64_t rowsMaterialized{};
    std::uint64_t firstPageReadyNs{};
    std::uint64_t firstPageRows{};
    std::uint64_t cancellations{};
};

class ISearchRepository {
public:
    virtual ~ISearchRepository() = default;

    virtual PreparedSearchResult PrepareByName(
        const std::wstring& nameTerm, int limit, wit::core::FileSort sort = {},
        bool caseSensitive = false) = 0;
    virtual PreparedSearchResult PrepareAdvanced(
        const AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort = {}) = 0;

    virtual int CountByName(const std::wstring& nameTerm, bool caseSensitive = false) = 0;
    virtual std::vector<wit::core::FileEntry> PageByName(
        const std::wstring& nameTerm,
        int offset,
        int limit,
        wit::core::FileSort sort = {},
        bool caseSensitive = false) = 0;
    virtual void CancelPending() = 0;
    virtual std::wstring LastErrorMessage() const = 0;
    virtual SearchMetrics LastMetrics() const { return {}; }
    virtual int CountAdvanced(const AdvancedSearchExpression& expression) = 0;
    virtual std::vector<wit::core::FileEntry> PageAdvanced(
        const AdvancedSearchExpression& expression,
        int offset,
        int limit,
        wit::core::FileSort sort = {}) = 0;
};

}
