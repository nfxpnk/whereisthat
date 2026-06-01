#pragma once

#include <cstdint>
#include "BrowserLocation.h"

namespace wit::core {

using CatalogId = std::uint64_t;

struct BrowserTarget {
    CatalogId catalogId{};
    BrowserLocation location;

    bool operator==(const BrowserTarget& other) const {
        return catalogId == other.catalogId && location == other.location;
    }
};

}

