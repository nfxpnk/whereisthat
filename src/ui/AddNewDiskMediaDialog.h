#pragma once
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

class AddNewDiskMediaDialog {
public:
    bool Show(HWND owner, HINSTANCE instance, AddNewDiskMediaResult& result);

private:
    HWND dialog_{};
    AddNewDiskMediaResult result_{};
    std::vector<std::wstring> drives_;

    static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam);
    INT_PTR HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
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
