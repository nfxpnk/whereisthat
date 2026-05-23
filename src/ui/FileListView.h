#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include "../core/FileEntry.h"
#include "../storage/Database.h"
namespace wit::ui {
class FileListView { public: HWND hwnd{}; std::int64_t catalogId{}; wit::storage::Database* db{}; int total{}; int pageStart{-1}; std::vector<wit::core::FileEntry> page; void Attach(HWND h){hwnd=h;} void SetCatalog(std::int64_t id, wit::storage::Database* database); void EnsurePage(int index); std::wstring TextFor(int row,int col); };
}
