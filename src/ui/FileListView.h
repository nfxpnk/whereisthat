#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include "../core/BrowserLocation.h"
#include "../core/Disk.h"
#include "../core/FileEntry.h"
#include "../storage/Database.h"

namespace wit::ui {
class FileListView {
public:
    HWND hwnd{};
    wit::core::BrowserLocation location;
    wit::storage::Database* db{};
    int total{};
    int pageStart{-1};
    std::vector<wit::core::FileEntry> page;
    std::vector<wit::core::Disk> diskPage;

    void Attach(HWND handle) { hwnd = handle; }
    void SetLocation(const wit::core::BrowserLocation& newLocation, wit::storage::Database* database);
    void EnsurePage(int index);
    bool ShowsDisks() const { return db && location.isRoot; }
    const wit::core::FileEntry* EntryAt(int row);
    const wit::core::Disk* DiskAt(int row);
    std::wstring TextFor(int row, int column);

private:
    void ConfigureColumns();
};
}
