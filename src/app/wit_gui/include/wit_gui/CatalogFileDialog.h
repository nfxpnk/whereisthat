#pragma once

#include <Windows.h>
#include <string>

namespace wit::ui {

class CatalogFileDialog {
public:
    bool ChooseNewCatalogPath(HWND owner, std::wstring& path) const;
    bool ChooseCatalogToOpen(HWND owner, std::wstring& path) const;
};

}

