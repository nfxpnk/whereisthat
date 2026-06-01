#pragma once
#include "wit_win32/BaseWindow.h"
#include "resource.h"
#include <Windows.h>
#include <string>
#include <vector>
#include "wit_types/CatalogIdentity.h"

namespace wit::ui {
enum class MediaSourceKind {
    None,
    Drive,
    Folder,
    Iso
};

struct AddNewDiskMediaResult {
    MediaSourceKind kind{MediaSourceKind::None};
    std::wstring originalPath;
    std::wstring scanRoot;
    std::wstring diskNumber;
    std::wstring diskName;
    bool calculateCrc{};
    bool browseArchives{};
    wit::core::CatalogId destinationCatalogId{};
    std::int64_t diskGroupId{};
};

struct DiskGroupChoice {
    std::int64_t id{};
    std::wstring name;
};

struct CatalogChoice {
    wit::core::CatalogId id{};
    std::wstring label;
    std::wstring path;
    std::vector<DiskGroupChoice> diskGroups;
};

class AddNewDiskMediaDialog : public ATL::CDialogImpl<AddNewDiskMediaDialog> {
public:
    enum { IDD = IDD_ADD_NEW_DISK_MEDIA };

    bool Show(HWND owner, HINSTANCE instance, const std::vector<CatalogChoice>& catalogs,
        wit::core::CatalogId preferredCatalogId, AddNewDiskMediaResult& result);

    BEGIN_MSG_MAP(AddNewDiskMediaDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_MEDIA_DRIVE_1, OnSelectDrive)
        COMMAND_ID_HANDLER(IDC_MEDIA_DRIVE_2, OnSelectDrive)
        COMMAND_ID_HANDLER(IDC_MEDIA_DRIVE_3, OnSelectDrive)
        COMMAND_ID_HANDLER(IDC_MEDIA_DRIVE_4, OnSelectDrive)
        COMMAND_ID_HANDLER(IDC_MEDIA_DRIVE_5, OnSelectDrive)
        COMMAND_ID_HANDLER(IDC_MEDIA_DRIVE_6, OnSelectDrive)
        COMMAND_ID_HANDLER(IDC_MEDIA_NETWORK, OnSelectFolder)
        COMMAND_ID_HANDLER(IDC_MEDIA_ISO, OnSelectIso)
        COMMAND_ID_HANDLER(IDC_MEDIA_CATALOG, OnCatalogChanged)
        COMMAND_ID_HANDLER(IDC_MEDIA_BROWSE_ARCHIVES, OnToggleArchives)
        COMMAND_ID_HANDLER(IDOK, OnConfirm)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

private:
    AddNewDiskMediaResult result_{};
    std::vector<std::wstring> drives_;
    std::vector<CatalogChoice> catalogs_;
    wit::core::CatalogId preferredCatalogId_{};

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSelectDrive(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnSelectFolder(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnSelectIso(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnCatalogChanged(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnToggleArchives(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnConfirm(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnCancel(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    void Initialize();
    void PopulateDriveButtons();
    void PopulateCatalogChoices();
    void PopulateDiskGroupChoices();
    void LayoutSourceButtons();
    void UpdateArchiveOptions();
    void UpdateConfirmEnabled();
    void SelectDrive(int index);
    void SelectFolder();
    void SelectIso();
    void ClearSelection();
    bool ApplySource(MediaSourceKind kind, const std::wstring& originalPath, const std::wstring& scanRoot,
        const std::wstring& status);
    bool Confirm();
};
}

