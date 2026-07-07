#pragma once
#include <optional>

namespace wit::core {

enum class FileSortColumn {
    Name,
    Type,
    Size,
    Path,
    Modified
};

struct FileSort {
    FileSortColumn column{FileSortColumn::Name};
    bool ascending{true};
};

struct BrowserRootSort {
    int column{};
    bool ascending{true};
};

inline std::optional<FileSortColumn> FileSortColumnFromListColumn(int column) {
    switch (column) {
    case 0: return FileSortColumn::Name;
    case 1: return FileSortColumn::Type;
    case 2: return FileSortColumn::Size;
    case 3: return FileSortColumn::Path;
    case 4: return FileSortColumn::Modified;
    default: return std::nullopt;
    }
}

inline int ListColumnFromFileSortColumn(FileSortColumn column) {
    switch (column) {
    case FileSortColumn::Type: return 1;
    case FileSortColumn::Size: return 2;
    case FileSortColumn::Path: return 3;
    case FileSortColumn::Modified: return 4;
    case FileSortColumn::Name:
    default: return 0;
    }
}

}
