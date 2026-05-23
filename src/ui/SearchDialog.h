#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "../core/FileEntry.h"
#include "../storage/Database.h"

namespace wit::ui {
class SearchDialog {
public:
    void Show(HWND owner, HINSTANCE instance, wit::storage::Database* database);

private:
    HWND dialog_{};
    HWND results_{};
    wit::storage::Database* db_{};
    std::wstring nameTerm_;
    int total_{};
    int pageStart_{-1};
    std::vector<wit::core::FileEntry> page_;

    static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam);
    INT_PTR HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Initialize();
    void Search();
    void EnsurePage(int row);
    std::wstring TextFor(int row, int column);
};
}
