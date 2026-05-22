#pragma once
#include <atlbase.h>
#include <atlwin.h>
#include <vector>
#include "../core/Catalog.h"
namespace wit::ui {
class CatalogListView { public: HWND hwnd{}; std::vector<wit::core::Catalog> catalogs; void Attach(HWND h){hwnd=h;} void Reload(); };
}
