#pragma once
#include "wit_win32/BaseWindow.h"
#include "resource.h"
#include <wit_types/FileEntry.h>
#include "wit_search/ISearchRepository.h"
#include <CommCtrl.h>
#include <functional>
#include <string>
#include <vector>

namespace wit::ui {
class SearchDialog : public ATL::CDialogImpl<SearchDialog>, public WTL::CDialogResize<SearchDialog> {
public:
    enum { IDD = IDD_SEARCH_ITEMS };

    bool Show(HWND owner, wit::search::ISearchRepository* search, std::function<void()> onClose);
    void Close();
    void RefreshDisplay();
    BOOL PreTranslateMessage(MSG* message);

    BEGIN_MSG_MAP(SearchDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnWindowClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        COMMAND_ID_HANDLER(IDC_SEARCH_EXECUTE, OnExecuteSearch)
        COMMAND_ID_HANDLER(IDCANCEL, OnCloseCommand)
        NOTIFY_HANDLER(IDC_SEARCH_RESULTS, LVN_GETDISPINFOW, OnGetDisplayInfo)
        NOTIFY_HANDLER(IDC_SEARCH_RESULTS, LVN_ODCACHEHINT, OnCacheHint)
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
    struct CachedPage {
        int start{};
        std::vector<wit::core::FileEntry> items;
        unsigned long long lastUsed{};
    };

    static constexpr int PageSize = 512;
    static constexpr std::size_t MaxCachedPages = 16;

    HWND results_{};
    HWND launchOwner_{};
    wit::search::ISearchRepository* search_{};
    std::function<void()> onClose_;
    std::wstring nameTerm_;
    int total_{};
    unsigned long long cacheClock_{};
    std::vector<CachedPage> cachedPages_;

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnExecuteSearch(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnWindowClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCloseCommand(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnGetDisplayInfo(int id, LPNMHDR header, BOOL& handled);
    LRESULT OnCacheHint(int id, LPNMHDR header, BOOL& handled);
    void Initialize();
    void Search();
    void ClearCache();
    void ResetResultItemCache();
    void CachePage(int pageStart);
    void PreloadRange(int firstRow, int lastRow);
    const wit::core::FileEntry* EntryAt(int row);
    void TextFor(int row, int column, wchar_t* buffer, std::size_t bufferSize);
};
}
