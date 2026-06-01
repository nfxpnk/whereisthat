#include <wit_infra/VolumeInfo.h>
#include <Windows.h>
#include <winioctl.h>

namespace wit::platform {
namespace {
bool QuerySeekPenalty(const wchar_t* volumeRoot, bool& incursSeekPenalty) {
    if (!volumeRoot || volumeRoot[0] == L'\0' || volumeRoot[1] != L':') return false;
    const std::wstring volumeDevice = L"\\\\.\\" + std::wstring(volumeRoot, 2);
    const auto handle = CreateFileW(volumeDevice.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;
    DEVICE_SEEK_PENALTY_DESCRIPTOR descriptor{};
    DWORD bytesReturned{};
    const bool queried = DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query), &descriptor, sizeof(descriptor), &bytesReturned, nullptr) != FALSE;
    CloseHandle(handle);
    if (!queried || bytesReturned < sizeof(descriptor) || descriptor.Size < sizeof(descriptor)) return false;
    incursSeekPenalty = descriptor.IncursSeekPenalty != FALSE;
    return true;
}
}

wit::core::DiskType ClassifyVolumeDiskType(wit::core::DiskType selectedType, bool removable, bool fixed,
    std::optional<bool> incursSeekPenalty) {
    if (selectedType == wit::core::DiskType::VirtualDisk) return selectedType;
    if (removable) return wit::core::DiskType::RemovableUSB;
    if (fixed && incursSeekPenalty) {
        return *incursSeekPenalty ? wit::core::DiskType::HardDisk : wit::core::DiskType::SolidStateDisk;
    }
    return selectedType;
}

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
    const auto driveType = GetDriveTypeW(metadataRoot);
    std::optional<bool> incursSeekPenalty;
    if (disk.diskType != wit::core::DiskType::VirtualDisk && driveType == DRIVE_FIXED) {
        bool value{};
        if (QuerySeekPenalty(metadataRoot, value)) incursSeekPenalty = value;
    }
    disk.diskType = ClassifyVolumeDiskType(disk.diskType, driveType == DRIVE_REMOVABLE,
        driveType == DRIVE_FIXED, incursSeekPenalty);
}

std::uint64_t FileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return 0;
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}
}
