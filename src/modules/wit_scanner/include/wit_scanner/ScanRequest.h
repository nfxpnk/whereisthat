#pragma once

#include <cstdint>
#include <string>
#include "wit_types/CatalogIdentity.h"

namespace wit::core {

enum class MediaSourceKind {
    Drive,
    Folder,
    Network,
    Iso
};

struct ScanRequest {
    wit::core::CatalogId destinationCatalogId{};
    MediaSourceKind kind{MediaSourceKind::Folder};
    std::wstring scanRoot;
    std::wstring originalPath;
    std::wstring diskName;
    std::wstring diskNumber;
    bool calculateCrc{};
    bool browseArchives{};
};

}
