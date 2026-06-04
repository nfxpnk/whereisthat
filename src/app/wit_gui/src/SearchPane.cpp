#include "wit_gui/SearchPane.h"
#include "wit_infra/StringUtils.h"
#include <wit_infra/Win32Helpers.h>
#include <CommCtrl.h>
#include <format>
#include <utility>

namespace wit::ui {
bool SearchDialog::Show(HWND owner, wit::search::ISearchRepository* search, std::function<void()> onClose) {
    if (!search) return false;
    launchOwner_ = owner;
    search_ = search;
    onClose_ = std::move(onClose);
    if (!m_hWnd && Create(nullptr) == nullptr) return false;
    ShowWindow(IsIconic() ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(m_hWnd);
    return true;
}

void SearchDialog::Close() {
    if (m_hWnd) DestroyWindow();
}

BOOL SearchDialog::PreTranslateMessage(MSG* message) {
    return m_hWnd != nullptr && IsDialogMessage(message);
}

LRESULT SearchDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    Initialize();
    DlgResize_Init(false, true);
    CenterWindow(launchOwner_);
    return TRUE;
}

LRESULT SearchDialog::OnExecuteSearch(WORD, WORD, HWND, BOOL&) {
    Search();
    return 0;
}

LRESULT SearchDialog::OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT SearchDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    results_ = nullptr;
    launchOwner_ = nullptr;
    search_ = nullptr;
    nameTerm_.clear();
    total_ = 0;
    pageStart_ = -1;
    page_.clear();
    auto onClose = std::move(onClose_);
    onClose_ = {};
    if (onClose) onClose();
    return 0;
}

LRESULT SearchDialog::OnCloseCommand(WORD, WORD, HWND, BOOL&) {
    DestroyWindow();
    return 0;
}

LRESULT SearchDialog::OnGetDisplayInfo(int, LPNMHDR header, BOOL&) {
    auto* displayInfo = reinterpret_cast<NMLVDISPINFOW*>(header);
    if (displayInfo->item.mask & LVIF_TEXT) {
        const auto text = TextFor(displayInfo->item.iItem, displayInfo->item.iSubItem);
        lstrcpynW(displayInfo->item.pszText, text.c_str(), displayInfo->item.cchTextMax);
    }
    return 0;
}

void SearchDialog::Initialize() {
    results_ = GetDlgItem(IDC_SEARCH_RESULTS);
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

    SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Enter part of a file or folder name to search.");
}

void SearchDialog::Search() {
    const int length = ::GetWindowTextLengthW(GetDlgItem(IDC_SEARCH_NAME));
    std::wstring term(static_cast<std::size_t>(length) + 1, L'\0');
    GetDlgItemTextW(IDC_SEARCH_NAME, term.data(), length + 1);
    term.resize(static_cast<std::size_t>(length));
    if (term.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        nameTerm_.clear();
        total_ = 0;
        pageStart_ = -1;
        page_.clear();
        ListView_SetItemCountEx(results_, 0, LVSICF_NOINVALIDATEALL);
        SetDlgItemTextW(IDC_SEARCH_SUMMARY, L"Enter a name to search for.");
        return;
    }

    nameTerm_ = term;
    if (!search_) return;
    total_ = search_->CountByName(nameTerm_);
    pageStart_ = -1;
    page_.clear();
    ListView_SetItemCountEx(results_, total_, LVSICF_NOINVALIDATEALL);
    ::InvalidateRect(results_, nullptr, TRUE);
    const auto summary = total_ == 0 ? std::wstring(L"No matching items found.") :
        std::format(L"{} matching item{}.", total_, total_ == 1 ? L"" : L"s");
    SetDlgItemTextW(IDC_SEARCH_SUMMARY, summary.c_str());
}

void SearchDialog::EnsurePage(int row) {
    if (!search_ || nameTerm_.empty()) return;
    constexpr int pageSize = 256;
    const int desiredPageStart = (row / pageSize) * pageSize;
    if (desiredPageStart == pageStart_) return;
    page_ = search_->PageByName(nameTerm_, desiredPageStart, pageSize);
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
        return file.isArchive ? L"Archive" : (file.isDirectory ? L"Folder" : file.extension);
    case 2:
        return file.isDirectory ? L"" : wit::core::FormatSize(file.size);
    case 3:
        return file.parentPath;
    case 4:
        return wit::platform::FormatUnixTimestamp(file.modifiedAt);
    default:
        return L"";
    }
}
}

