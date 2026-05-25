#include "VolumeInfo.h"
#include <Windows.h>

namespace wit::platform {
void PopulateVolumeMetadata(const std::wstring& scanRoot, wit::core::Disk& disk) {
    wchar_t volumeRoot[MAX_PATH]{};
    const auto* metadataRoot = scanRoot.c_str();
    if (GetVolumePathNameW(scanRoot.c_str(), volumeRoot, MAX_PATH)) {
        metadataRoot = volumeRoot;
    }
    wchar_t volumeLabel[MAX_PATH]{};
    wchar_t fileSystem[MAX_PATH]{};
    DWORD serial{};
    if (GetVolumeInformationW(metadataRoot, volumeLabel, MAX_PATH, &serial, nullptr, nullptr,
        fileSystem, MAX_PATH)) {
        disk.volumeLabel = volumeLabel;
        disk.fileSystem = fileSystem;
        disk.serialNumber = std::to_wstring(serial);
    }
    ULARGE_INTEGER freeAvailable{}, capacity{}, freeSpace{};
    if (GetDiskFreeSpaceExW(metadataRoot, &freeAvailable, &capacity, &freeSpace)) {
        disk.totalCapacity = capacity.QuadPart;
        disk.freeSpace = freeSpace.QuadPart;
    }
    DWORD sectorsPerCluster{}, bytesPerSector{}, freeClusters{}, totalClusters{};
    if (GetDiskFreeSpaceW(metadataRoot, &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters)) {
        disk.clusterSize = static_cast<std::uint64_t>(sectorsPerCluster) * bytesPerSector;
    }
    if (disk.diskType != wit::core::DiskType::VirtualDisk &&
        GetDriveTypeW(metadataRoot) == DRIVE_REMOVABLE) {
        disk.diskType = wit::core::DiskType::RemovableUSB;
    }
}

std::uint64_t FileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return 0;
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}
}
