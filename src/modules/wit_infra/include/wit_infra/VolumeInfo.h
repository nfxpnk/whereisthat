#pragma once
#include <optional>
#include <string>
#include <wit_types/Disk.h>

namespace wit::platform {
wit::core::DiskType ClassifyVolumeDiskType(wit::core::DiskType selectedType, bool removable, bool fixed,
    bool optical, std::optional<bool> incursSeekPenalty);
void PopulateVolumeMetadata(const std::wstring& scanRoot, wit::core::Disk& disk);
std::uint64_t FileSize(const std::wstring& path);
}
