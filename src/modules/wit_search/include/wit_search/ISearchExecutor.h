#pragma once
#include <vector>
#include "wit_types/SearchQuery.h"
#include "wit_search/SearchResult.h"

namespace wit::search {
class ISearchExecutor {
public:
    virtual ~ISearchExecutor() = default;
    virtual std::vector<SearchResult> Execute(const SearchQuery& query, int offset, int limit) = 0;
};
}

