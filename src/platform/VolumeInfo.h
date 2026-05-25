#pragma once
#include <string>
#include "../core/Disk.h"

namespace wit::platform {
void PopulateVolumeMetadata(const std::wstring& scanRoot, wit::core::Disk& disk);
std::uint64_t FileSize(const std::wstring& path);
}
