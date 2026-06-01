#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "wit_types/BrowserLocation.h"
#include "wit_catalog/Catalog.h"
#include <wit_types/Disk.h>
#include <wit_types/FileEntry.h>
#include "wit_types/FolderEntry.h"

namespace wit::storage {
class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual bool IsOpen() const = 0;
    virtual bool IsEditable() const = 0;
    virtual wit::core::CatalogMetadata GetCatalogMetadata() = 0;
    virtual wit::core::CatalogSummary GetCatalogSummary() const = 0;
    virtual std::vector<wit::core::Catalog> GetCatalogs() = 0;
};
}

