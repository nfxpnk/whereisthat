#pragma once
#include "../app/WtlSupport.h"
#include "../app/resource.h"
#include "../core/FileEntry.h"
#include "../storage/Database.h"
#include <CommCtrl.h>
#include <string>
#include <vector>

namespace wit::ui {
class SearchDialog : public ATL::CDialogImpl<SearchDialog> {
public:
    enum { IDD = IDD_SEARCH_ITEMS };

    void Show(HWND owner, wit::storage::Database* database);

    BEGIN_MSG_MAP(SearchDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_SEARCH_EXECUTE, OnExecuteSearch)
        COMMAND_ID_HANDLER(IDCANCEL, OnClose)
        NOTIFY_HANDLER(IDC_SEARCH_RESULTS, LVN_GETDISPINFOW, OnGetDisplayInfo)
    END_MSG_MAP()

private:
    HWND results_{};
    wit::storage::Database* db_{};
    std::wstring nameTerm_;
    int total_{};
    int pageStart_{-1};
    std::vector<wit::core::FileEntry> page_;

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnExecuteSearch(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnClose(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnGetDisplayInfo(int id, LPNMHDR header, BOOL& handled);
    void Initialize();
    void Search();
    void EnsurePage(int row);
    std::wstring TextFor(int row, int column);
};
}
