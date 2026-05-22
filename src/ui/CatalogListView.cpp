#include "CatalogListView.h"
#include <CommCtrl.h>
namespace wit::ui { void CatalogListView::Reload(){ ListView_DeleteAllItems(hwnd); for(size_t i=0;i<catalogs.size();++i){ LVITEMW it{}; it.mask=LVIF_TEXT|LVIF_PARAM; it.iItem=(int)i; it.pszText=(LPWSTR)catalogs[i].name.c_str(); it.lParam=(LPARAM)catalogs[i].id; ListView_InsertItem(hwnd,&it);} } }
