#pragma once
#include "../app/WtlSupport.h"
#include "../app/resource.h"
#include "../core/FileEntry.h"
#include "../storage/Database.h"
#include <CommCtrl.h>
#include <functional>
#include <string>
#include <vector>

namespace wit::ui {
class SearchDialog : public ATL::CDialogImpl<SearchDialog>, public WTL::CDialogResize<SearchDialog> {
public:
    enum { IDD = IDD_SEARCH_ITEMS };

    bool Show(HWND owner, wit::storage::Database* database, std::function<void()> onClose);
    void Close();
    BOOL PreTranslateMessage(MSG* message);

    BEGIN_MSG_MAP(SearchDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnWindowClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        COMMAND_ID_HANDLER(IDC_SEARCH_EXECUTE, OnExecuteSearch)
        COMMAND_ID_HANDLER(IDCANCEL, OnCloseCommand)
        NOTIFY_HANDLER(IDC_SEARCH_RESULTS, LVN_GETDISPINFOW, OnGetDisplayInfo)
        CHAIN_MSG_MAP(WTL::CDialogResize<SearchDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(SearchDialog)
        DLGRESIZE_CONTROL(IDC_SEARCH_NAME, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SEARCH_EXECUTE, DLSZ_MOVE_X)
        DLGRESIZE_CONTROL(IDC_SEARCH_SUMMARY, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SEARCH_RESULTS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

private:
    HWND results_{};
    HWND launchOwner_{};
    wit::storage::Database* db_{};
    std::function<void()> onClose_;
    std::wstring nameTerm_;
    int total_{};
    int pageStart_{-1};
    std::vector<wit::core::FileEntry> page_;

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnExecuteSearch(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnWindowClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCloseCommand(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnGetDisplayInfo(int id, LPNMHDR header, BOOL& handled);
    void Initialize();
    void Search();
    void EnsurePage(int row);
    std::wstring TextFor(int row, int column);
};
}
