#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace wit::core {
enum class DiskType {
    CD,
    DVD,
    BluRay,
    HardDisk,
    SolidStateDisk,
    RemovableUSB,
    VirtualDisk,
    Other
};

inline const char* DiskTypeValue(DiskType type) {
    switch (type) {
    case DiskType::CD: return "CD";
    case DiskType::DVD: return "DVD";
    case DiskType::BluRay: return "BluRay";
    case DiskType::HardDisk: return "HardDisk";
    case DiskType::SolidStateDisk: return "SolidStateDisk";
    case DiskType::RemovableUSB: return "RemovableUSB";
    case DiskType::VirtualDisk: return "VirtualDisk";
    default: return "Other";
    }
}

struct Disk {
    std::int64_t id{};
    std::int64_t diskGroupId{};
    std::wstring diskName;
    std::int64_t diskNumber{};
    std::wstring sourcePath;
    std::wstring volumeLabel;
    std::uint64_t totalCapacity{};
    std::uint64_t freeSpace{};
    std::uint64_t clusterSize{};
    std::wstring serialNumber;
    std::wstring fileSystem;
    std::int64_t totalFiles{};
    std::int64_t totalFolders{};
    std::int64_t addedAt{};
    std::int64_t updatedAt{};
    std::wstring description;
    std::wstring category;
    std::wstring location;
    DiskType diskType{DiskType::Other};
};

struct DiskGroup {
    std::int64_t id{};
    std::wstring name;
    std::int64_t createdAt{};
    std::int64_t updatedAt{};
    std::int64_t totalDisks{};
};

enum class BrowserItemType {
    DiskGroup,
    Disk
};

struct BrowserItem {
    BrowserItemType type{BrowserItemType::Disk};
    DiskGroup group;
    Disk disk;
};

struct DiskScanStatistics {
    std::int64_t diskId{};
    std::int64_t lastScannedAt{};
    std::int64_t imageScanningTimeMs{};
    std::int64_t importedDescriptionsCount{};
    bool calculatedFileCrcs{};
    std::int64_t scannedArchives{};
    std::int64_t archiveFilesCount{};
    std::int64_t archiveFoldersCount{};
};

struct CatalogMetadata {
    std::wstring description;
};

struct CatalogSummary {
    std::uint64_t catalogFileSize{};
    std::int64_t totalDisks{};
    std::int64_t totalFiles{};
    std::int64_t totalFolders{};
    std::uint64_t totalStorageSpace{};
    std::uint64_t totalUsedSpace{};
};
}

