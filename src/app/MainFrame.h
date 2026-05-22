#pragma once
#include <Windows.h>
#include <CommCtrl.h>
#include <thread>
#include "../storage/Database.h"
#include "../core/FileScanner.h"
#include "../ui/CatalogListView.h"
#include "../ui/FileListView.h"
#include "resource.h"

class MainFrame {
public:
    bool Create();
    void Show(int command);
private:
    static constexpr wchar_t kClassName[] = L"WhereIsThatMainFrame";
    HWND hwnd_{};
    HWND catalogsCtl_{};
    HWND filesCtl_{};
    HWND status_{};
    wit::storage::Database db_;
    wit::ui::CatalogListView catalogs_;
    wit::ui::FileListView files_;
    std::thread worker_;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    bool OnCreate();
    void OnSize(int width, int height);
    void OnDestroy();
    void OnCommand(int id);
    void OnNewCatalog();
    void OnRefresh();
    void OnExit();
    void OnAbout();
    LRESULT OnCatalogChanged(LPNMHDR hdr);
    LRESULT OnFileGetDispInfo(LPNMHDR hdr);
    void ReloadCatalogs();
};
