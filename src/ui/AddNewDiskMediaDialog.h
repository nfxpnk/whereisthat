#pragma once
#include "../app/WtlSupport.h"
#include "../app/resource.h"
#include <Windows.h>
#include <string>
#include <vector>

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
};

class AddNewDiskMediaDialog : public ATL::CDialogImpl<AddNewDiskMediaDialog> {
public:
    enum { IDD = IDD_ADD_NEW_DISK_MEDIA };

    bool Show(HWND owner, HINSTANCE instance, AddNewDiskMediaResult& result);

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
        COMMAND_ID_HANDLER(IDC_MEDIA_BROWSE_ARCHIVES, OnToggleArchives)
        COMMAND_ID_HANDLER(IDOK, OnConfirm)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

private:
    AddNewDiskMediaResult result_{};
    std::vector<std::wstring> drives_;

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSelectDrive(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnSelectFolder(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnSelectIso(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnToggleArchives(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnConfirm(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    LRESULT OnCancel(WORD notifyCode, WORD id, HWND control, BOOL& handled);
    void Initialize();
    void PopulateDriveButtons();
    void LayoutSourceButtons();
    void UpdateArchiveOptions();
    void SelectDrive(int index);
    void SelectFolder();
    void SelectIso();
    void ClearSelection();
    bool ApplySource(MediaSourceKind kind, const std::wstring& originalPath, const std::wstring& scanRoot,
        const std::wstring& status);
    bool Confirm();
};
}
