#pragma once
#include <optional>
#include <string>
#include "../core/Disk.h"

namespace wit::platform {
wit::core::DiskType ClassifyVolumeDiskType(wit::core::DiskType selectedType, bool removable, bool fixed,
    std::optional<bool> incursSeekPenalty);
void PopulateVolumeMetadata(const std::wstring& scanRoot, wit::core::Disk& disk);
std::uint64_t FileSize(const std::wstring& path);
}
