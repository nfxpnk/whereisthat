#pragma once
#include <vector>
#include "wit_scanner/IMetadataExtractor.h"

namespace wit::extractors {
std::vector<wit::scanner::IMetadataExtractor*> GetExtractors();
}

