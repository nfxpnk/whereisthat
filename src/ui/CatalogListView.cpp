#include "CatalogListView.h"
#include <CommCtrl.h>

namespace wit::ui {
void CatalogListView::Reload() {
    ListView_DeleteAllItems(hwnd);
    for (std::size_t index = 0; index < catalogs.size(); ++index) {
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(index);
        item.pszText = const_cast<LPWSTR>(catalogs[index].name.c_str());
        item.lParam = static_cast<LPARAM>(catalogs[index].id);
        ListView_InsertItem(hwnd, &item);
    }
}
}
