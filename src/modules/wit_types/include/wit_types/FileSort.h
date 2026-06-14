#pragma once

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

}
