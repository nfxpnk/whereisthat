#include "FileListView.h"
#include "../core/SizeFormat.h"
#include <CommCtrl.h>
namespace wit::ui {
void FileListView::SetCatalog(std::int64_t id, wit::storage::Database* database){ catalogId=id; db=database; total=db?db->GetFileCountForCatalog(id):0; pageStart=-1; page.clear(); ListView_SetItemCountEx(hwnd,total,LVSICF_NOINVALIDATEALL); InvalidateRect(hwnd,nullptr,TRUE);} 
void FileListView::EnsurePage(int index){ if(!db) return; const int pageSize=256; int desired=(index/pageSize)*pageSize; if(desired==pageStart) return; page=db->GetFilesPageForCatalog(catalogId,desired,pageSize); pageStart=desired; }
std::wstring FileListView::TextFor(int row,int col){ EnsurePage(row); int i=row-pageStart; if(i<0||i>=(int)page.size()) return L""; auto& f=page[i]; switch(col){case 0:return f.name;case 1:return f.isDirectory?L"Folder":f.extension;case 2:return f.isDirectory?L"":wit::core::FormatSize(f.size);case 3:return f.parentPath;case 4:return f.modifiedAt;default:return L"";} }
}
