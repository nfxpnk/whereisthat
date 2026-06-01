#pragma once
#include <vector>
#include "wit_catalog/Catalog.h"

namespace wit::catalog {
class ICatalogRepository {
public:
    virtual ~ICatalogRepository() = default;
    virtual std::vector<wit::core::Catalog> GetCatalogs() = 0;
};
}

