#pragma once

#include <string>
#include <vector>

#include "wit_types/FileEntry.h"
#include "wit_types/FileSort.h"

namespace wit::search {

class ISearchRepository {
public:
    virtual ~ISearchRepository() = default;

    virtual int CountByName(const std::wstring& nameTerm) = 0;
    virtual std::vector<wit::core::FileEntry> PageByName(
        const std::wstring& nameTerm,
        int offset,
        int limit,
        wit::core::FileSort sort = {}) = 0;
};

}
