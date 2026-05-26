#include "CatalogFileDialog.h"
#include <shobjidl.h>

namespace wit::ui {
namespace {

constexpr COMDLG_FILTERSPEC kCatalogFileTypes[] = {
    {L"SQLite catalog database (*.db)", L"*.db"},
    {L"All files (*.*)", L"*.*"}
};

bool ItemFileSystemPath(IShellItem* item, std::wstring& path) {
    PWSTR selected{};
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &selected))) return false;
    path = selected;
    CoTaskMemFree(selected);
    return true;
}

}

bool CatalogFileDialog::ChooseNewCatalogPath(HWND owner, std::wstring& path) const {
    IFileSaveDialog* dialog{};
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return false;
    }
    dialog->SetTitle(L"Create a new catalog database");
    dialog->SetFileTypes(2, kCatalogFileTypes);
    dialog->SetFileTypeIndex(1);
    dialog->SetDefaultExtension(L"db");
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_OVERWRITEPROMPT);
    bool selected = false;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item{};
        if (SUCCEEDED(dialog->GetResult(&item))) {
            selected = ItemFileSystemPath(item, path);
            item->Release();
        }
    }
    dialog->Release();
    return selected;
}

bool CatalogFileDialog::ChooseCatalogToOpen(HWND owner, std::wstring& path) const {
    IFileOpenDialog* dialog{};
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return false;
    }
    dialog->SetTitle(L"Open a catalog database");
    dialog->SetFileTypes(2, kCatalogFileTypes);
    dialog->SetFileTypeIndex(1);
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
    bool selected = false;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item{};
        if (SUCCEEDED(dialog->GetResult(&item))) {
            selected = ItemFileSystemPath(item, path);
            item->Release();
        }
    }
    dialog->Release();
    return selected;
}

}
