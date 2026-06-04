#pragma once

#include <string>
#include <vector>

#include "wit_types/FileEntry.h"

namespace wit::search {

class ISearchRepository {
public:
    virtual ~ISearchRepository() = default;

    virtual int CountByName(const std::wstring& nameTerm) = 0;
    virtual std::vector<wit::core::FileEntry> PageByName(
        const std::wstring& nameTerm,
        int offset,
        int limit) = 0;
};

}
