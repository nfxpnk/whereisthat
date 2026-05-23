#include "SearchDialog.h"
#include "../app/resource.h"
#include "../core/SizeFormat.h"
#include <CommCtrl.h>

namespace wit::ui {
void SearchDialog::Show(HWND owner, HINSTANCE instance, wit::storage::Database* database) {
    db_ = database;
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SEARCH_ITEMS), owner, DialogProc,
        reinterpret_cast<LPARAM>(this));
}

INT_PTR CALLBACK SearchDialog::DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<SearchDialog*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (message == WM_INITDIALOG) {
        self = reinterpret_cast<SearchDialog*>(lparam);
        self->dialog_ = dialog;
        SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(message, wparam, lparam) : FALSE;
}

INT_PTR SearchDialog::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_INITDIALOG:
        Initialize();
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_SEARCH_EXECUTE) {
            Search();
            return TRUE;
        }
        if (LOWORD(wparam) == IDCANCEL) {
            EndDialog(dialog_, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<LPNMHDR>(lparam);
        if (header->idFrom == IDC_SEARCH_RESULTS && header->code == LVN_GETDISPINFOW) {
            auto* displayInfo = reinterpret_cast<NMLVDISPINFOW*>(lparam);
            if (displayInfo->item.mask & LVIF_TEXT) {
                const auto text = TextFor(displayInfo->item.iItem, displayInfo->item.iSubItem);
                lstrcpynW(displayInfo->item.pszText, text.c_str(), displayInfo->item.cchTextMax);
            }
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

void SearchDialog::Initialize() {
    results_ = GetDlgItem(dialog_, IDC_SEARCH_RESULTS);
    ListView_SetExtendedListViewStyle(results_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH | LVCF_FMT};
    column.fmt = LVCFMT_LEFT;
    column.cx = 145;
    column.pszText = const_cast<LPWSTR>(L"Name");
    ListView_InsertColumn(results_, 0, &column);
    column.cx = 66;
    column.pszText = const_cast<LPWSTR>(L"Type");
    ListView_InsertColumn(results_, 1, &column);
    column.fmt = LVCFMT_RIGHT;
    column.cx = 76;
    column.pszText = const_cast<LPWSTR>(L"Size");
    ListView_InsertColumn(results_, 2, &column);
    column.fmt = LVCFMT_LEFT;
    column.cx = 170;
    column.pszText = const_cast<LPWSTR>(L"Path");
    ListView_InsertColumn(results_, 3, &column);
    column.cx = 105;
    column.pszText = const_cast<LPWSTR>(L"Modified");
    ListView_InsertColumn(results_, 4, &column);

    SetDlgItemTextW(dialog_, IDC_SEARCH_SUMMARY, L"Enter part of a file or folder name to search.");
}

void SearchDialog::Search() {
    const int length = GetWindowTextLengthW(GetDlgItem(dialog_, IDC_SEARCH_NAME));
    std::wstring term(static_cast<std::size_t>(length) + 1, L'\0');
    GetDlgItemTextW(dialog_, IDC_SEARCH_NAME, term.data(), length + 1);
    term.resize(static_cast<std::size_t>(length));
    if (term.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        nameTerm_.clear();
        total_ = 0;
        pageStart_ = -1;
        page_.clear();
        ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
        SetDlgItemTextW(dialog_, IDC_SEARCH_SUMMARY, L"Enter a name to search for.");
        return;
    }

    nameTerm_ = term;
    total_ = db_->GetItemSearchCount(nameTerm_);
    pageStart_ = -1;
    page_.clear();
    ListView_SetItemCountEx(results_, total_, LVSICF_NOINVALIDATEALL);
    InvalidateRect(results_, nullptr, TRUE);
    const auto summary = total_ == 0 ? std::wstring(L"No matching items found.") :
        std::to_wstring(total_) + (total_ == 1 ? L" matching item." : L" matching items.");
    SetDlgItemTextW(dialog_, IDC_SEARCH_SUMMARY, summary.c_str());
}

void SearchDialog::EnsurePage(int row) {
    if (!db_ || nameTerm_.empty()) return;
    constexpr int pageSize = 256;
    const int desiredPageStart = (row / pageSize) * pageSize;
    if (desiredPageStart == pageStart_) return;
    page_ = db_->GetItemSearchPage(nameTerm_, desiredPageStart, pageSize);
    pageStart_ = desiredPageStart;
}

std::wstring SearchDialog::TextFor(int row, int column) {
    EnsurePage(row);
    const int index = row - pageStart_;
    if (index < 0 || index >= static_cast<int>(page_.size())) return L"";
    const auto& file = page_[index];
    switch (column) {
    case 0:
        return file.name;
    case 1:
        return file.isDirectory ? L"Folder" : file.extension;
    case 2:
        return file.isDirectory ? L"" : wit::core::FormatSize(file.size);
    case 3:
        return file.parentPath;
    case 4:
        return file.modifiedAt;
    default:
        return L"";
    }
}
}
