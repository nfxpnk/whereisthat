#pragma once
#include <atlbase.h>
#include <atlapp.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include <thread>
#include "../storage/Database.h"
#include "../core/FileScanner.h"
#include "../ui/CatalogListView.h"
#include "../ui/FileListView.h"
#include "resource.h"

class MainFrame : public CFrameWindowImpl<MainFrame> {
public:
    DECLARE_FRAME_WND_CLASS(L"WhereIsThatMainFrame", IDR_MAINMENU)
    BEGIN_MSG_MAP(MainFrame)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_SIZE(OnSize)
        MSG_WM_DESTROY(OnDestroy)
        COMMAND_ID_HANDLER(ID_FILE_NEWCATALOG, OnNewCatalog)
        COMMAND_ID_HANDLER(ID_FILE_REFRESH, OnRefresh)
        COMMAND_ID_HANDLER(ID_FILE_EXIT, OnExit)
        COMMAND_ID_HANDLER(ID_HELP_ABOUT, OnAbout)
        NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnCatalogChanged)
        NOTIFY_CODE_HANDLER(LVN_GETDISPINFOW, OnFileGetDispInfo)
        CHAIN_MSG_MAP(CFrameWindowImpl<MainFrame>)
    END_MSG_MAP()
private:
    CListViewCtrl catalogsCtl_; CListViewCtrl filesCtl_; CStatusBarCtrl status_; wit::storage::Database db_; wit::ui::CatalogListView catalogs_; wit::ui::FileListView files_; std::thread worker_;
    int OnCreate(LPCREATESTRUCT);
    void OnSize(UINT, CSize);
    void OnDestroy();
    LRESULT OnNewCatalog(WORD, WORD, HWND, BOOL&);
    LRESULT OnRefresh(WORD, WORD, HWND, BOOL&);
    LRESULT OnExit(WORD, WORD, HWND, BOOL&);
    LRESULT OnAbout(WORD, WORD, HWND, BOOL&);
    LRESULT OnCatalogChanged(int, LPNMHDR, BOOL&);
    LRESULT OnFileGetDispInfo(int, LPNMHDR hdr, BOOL&);
    void ReloadCatalogs();
};
