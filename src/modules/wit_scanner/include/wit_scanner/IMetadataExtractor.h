#pragma once
#include <string>

namespace wit::scanner {
class IMetadataExtractor {
public:
    virtual ~IMetadataExtractor() = default;
    virtual bool CanExtract(const std::wstring& path) const = 0;
};
}

