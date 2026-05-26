#include "AddNewDiskMediaDialog.h"
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cwctype>
#include <shellapi.h>
#include <shobjidl.h>
#include <vector>

namespace wit::ui {
namespace {
constexpr std::array<int, 6> kDriveControls{
    IDC_MEDIA_DRIVE_1, IDC_MEDIA_DRIVE_2, IDC_MEDIA_DRIVE_3,
    IDC_MEDIA_DRIVE_4, IDC_MEDIA_DRIVE_5, IDC_MEDIA_DRIVE_6
};
constexpr std::array<int, 3> kArchiveChildControls{
    IDC_MEDIA_ARCHIVE_DESCRIPTIONS_ONLY,
    IDC_MEDIA_SKIP_UNCHANGED_ARCHIVES,
    IDC_MEDIA_IMPORT_ARCHIVE_DESCRIPTIONS
};
constexpr int kSourceButtonSize = 42;
constexpr int kSourceButtonGap = 6;
constexpr int kSourceButtonLeft = 12;
constexpr int kSourceButtonTop = 11;
constexpr COMDLG_FILTERSPEC kIsoFileTypes[] = {
    {L"ISO disk images (*.iso)", L"*.iso"},
    {L"All files (*.*)", L"*.*"}
};

std::wstring ReadText(HWND dialog, int control) {
    const auto length = GetWindowTextLengthW(GetDlgItem(dialog, control));
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetDlgItemTextW(dialog, control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

bool IsBlank(const std::wstring& value) {
    for (const auto character : value) {
        if (!std::iswspace(character)) return false;
    }
    return true;
}

bool ShellItemPath(IShellItem* item, std::wstring& path) {
    PWSTR selected{};
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &selected))) return false;
    path = selected;
    CoTaskMemFree(selected);
    return true;
}

bool IsDirectory(const std::wstring& path) {
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring SourceName(const std::wstring& path) {
    wchar_t volume[MAX_PATH]{};
    if (path.size() >= 3 && path[1] == L':' &&
        GetVolumeInformationW(path.c_str(), volume, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0) &&
        volume[0] != L'\0') {
        return volume;
    }
    auto end = path.find_last_not_of(L"\\/");
    if (end == std::wstring::npos) return path;
    const auto separator = path.find_last_of(L"\\/", end);
    return separator == std::wstring::npos ? path.substr(0, end + 1)
        : path.substr(separator + 1, end - separator);
}

std::vector<std::wstring> CdRomRoots() {
    std::vector<std::wstring> roots;
    const auto drives = GetLogicalDrives();
    for (int index = 0; index < 26; ++index) {
        if ((drives & (1UL << index)) == 0) continue;
        std::wstring root{static_cast<wchar_t>(L'A' + index), L':', L'\\'};
        if (GetDriveTypeW(root.c_str()) == DRIVE_CDROM) roots.push_back(root);
    }
    return roots;
}

bool HasRoot(const std::vector<std::wstring>& roots, const std::wstring& candidate) {
    for (const auto& root : roots) {
        if (_wcsicmp(root.c_str(), candidate.c_str()) == 0) return true;
    }
    return false;
}

bool ChooseFolder(HWND owner, std::wstring& path) {
    IFileOpenDialog* picker{};
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker)))) {
        return false;
    }
    picker->SetTitle(L"Select network or computer folder to scan");
    DWORD options{};
    picker->GetOptions(&options);
    picker->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    bool accepted = false;
    if (SUCCEEDED(picker->Show(owner))) {
        IShellItem* item{};
        if (SUCCEEDED(picker->GetResult(&item))) {
            accepted = ShellItemPath(item, path);
            item->Release();
        }
    }
    picker->Release();
    return accepted;
}

bool ChooseIso(HWND owner, std::wstring& path) {
    IFileOpenDialog* picker{};
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker)))) {
        return false;
    }
    picker->SetTitle(L"Select ISO image to scan");
    picker->SetFileTypes(2, kIsoFileTypes);
    picker->SetFileTypeIndex(1);
    DWORD options{};
    picker->GetOptions(&options);
    picker->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
    bool accepted = false;
    if (SUCCEEDED(picker->Show(owner))) {
        IShellItem* item{};
        if (SUCCEEDED(picker->GetResult(&item))) {
            accepted = ShellItemPath(item, path);
            item->Release();
        }
    }
    picker->Release();
    return accepted;
}

bool MountIso(const std::wstring& path, std::wstring& mountedRoot) {
    const auto priorRoots = CdRomRoots();
    SHELLEXECUTEINFOW execute{sizeof(execute)};
    execute.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"mount";
    execute.lpFile = path.c_str();
    execute.nShow = SW_HIDE;
    if (!ShellExecuteExW(&execute)) return false;
    if (execute.hProcess) {
        WaitForSingleObject(execute.hProcess, 2000);
        CloseHandle(execute.hProcess);
    }
    for (int attempt = 0; attempt < 20; ++attempt) {
        const auto currentRoots = CdRomRoots();
        for (const auto& root : currentRoots) {
            if (!HasRoot(priorRoots, root) && IsDirectory(root)) {
                mountedRoot = root;
                return true;
            }
        }
        Sleep(100);
    }
    return false;
}
}

bool AddNewDiskMediaDialog::Show(HWND owner, HINSTANCE, const std::vector<CatalogChoice>& catalogs,
    wit::core::CatalogId preferredCatalogId, AddNewDiskMediaResult& result) {
    result_ = {};
    catalogs_ = catalogs;
    preferredCatalogId_ = preferredCatalogId;
    const auto status = DoModal(owner);
    if (status != IDOK) return false;
    result = result_;
    return true;
}

LRESULT AddNewDiskMediaDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    Initialize();
    return TRUE;
}

LRESULT AddNewDiskMediaDialog::OnSelectDrive(WORD notifyCode, WORD id, HWND, BOOL&) {
    if (notifyCode == BN_CLICKED) SelectDrive(id - IDC_MEDIA_DRIVE_1);
    return 0;
}

LRESULT AddNewDiskMediaDialog::OnSelectFolder(WORD notifyCode, WORD, HWND, BOOL&) {
    if (notifyCode == BN_CLICKED) SelectFolder();
    return 0;
}

LRESULT AddNewDiskMediaDialog::OnSelectIso(WORD notifyCode, WORD, HWND, BOOL&) {
    if (notifyCode == BN_CLICKED) SelectIso();
    return 0;
}

LRESULT AddNewDiskMediaDialog::OnToggleArchives(WORD notifyCode, WORD, HWND, BOOL&) {
    if (notifyCode == BN_CLICKED) UpdateArchiveOptions();
    return 0;
}

LRESULT AddNewDiskMediaDialog::OnConfirm(WORD, WORD, HWND, BOOL&) {
    if (Confirm()) EndDialog(IDOK);
    return 0;
}

LRESULT AddNewDiskMediaDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
    EndDialog(IDCANCEL);
    return 0;
}

void AddNewDiskMediaDialog::Initialize() {
    RECT bounds{};
    ::GetWindowRect(m_hWnd, &bounds);
    ::SetWindowPos(m_hWnd, nullptr, 0, 0, 510, bounds.bottom - bounds.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    ::CheckDlgButton(m_hWnd, IDC_MEDIA_REDISPLAY, BST_CHECKED);
    ::CheckDlgButton(m_hWnd, IDC_MEDIA_SKIP_UNCHANGED_ARCHIVES, BST_CHECKED);
    ::EnableWindow(::GetDlgItem(m_hWnd, IDOK), FALSE);
    ::SetDlgItemTextW(m_hWnd, IDC_MEDIA_STATUS, L"No drive selected");
    PopulateDriveButtons();
    PopulateCatalogChoices();
    UpdateArchiveOptions();
}

void AddNewDiskMediaDialog::PopulateDriveButtons() {
    drives_.clear();
    const auto available = GetLogicalDrives();
    for (int index = 0; index < 26 && drives_.size() < kDriveControls.size(); ++index) {
        if ((available & (1UL << index)) == 0) continue;
        std::wstring root{static_cast<wchar_t>(L'A' + index), L':', L'\\'};
        const auto type = GetDriveTypeW(root.c_str());
        if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) drives_.push_back(root);
    }
    for (std::size_t index = 0; index < kDriveControls.size(); ++index) {
        const auto button = ::GetDlgItem(m_hWnd, kDriveControls[index]);
        if (index < drives_.size()) {
            const auto text = drives_[index].substr(0, 2);
            ::SetWindowTextW(button, text.c_str());
            ::ShowWindow(button, SW_SHOW);
            ::EnableWindow(button, TRUE);
        } else {
            ::ShowWindow(button, SW_HIDE);
            ::EnableWindow(button, FALSE);
        }
    }
    LayoutSourceButtons();
}

void AddNewDiskMediaDialog::PopulateCatalogChoices() {
    const auto combo = ::GetDlgItem(m_hWnd, IDC_MEDIA_CATALOG);
    int selected = -1;
    for (std::size_t index = 0; index < catalogs_.size(); ++index) {
        auto label = catalogs_[index].label + L" - " + catalogs_[index].path;
        const auto position = static_cast<int>(::SendMessageW(combo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(label.c_str())));
        if (position >= 0) {
            ::SendMessageW(combo, CB_SETITEMDATA, position, static_cast<LPARAM>(index));
            if (catalogs_[index].id == preferredCatalogId_) selected = position;
        }
    }
    if (selected < 0 && !catalogs_.empty()) selected = 0;
    if (selected >= 0) ::SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

void AddNewDiskMediaDialog::LayoutSourceButtons() {
    int left = kSourceButtonLeft;
    for (std::size_t index = 0; index < drives_.size(); ++index) {
        ::SetWindowPos(::GetDlgItem(m_hWnd, kDriveControls[index]), nullptr, left, kSourceButtonTop,
            kSourceButtonSize, kSourceButtonSize, SWP_NOZORDER | SWP_NOACTIVATE);
        left += kSourceButtonSize + kSourceButtonGap;
    }
    for (const int control : {IDC_MEDIA_ISO, IDC_MEDIA_NETWORK}) {
        ::SetWindowPos(::GetDlgItem(m_hWnd, control), nullptr, left, kSourceButtonTop,
            kSourceButtonSize, kSourceButtonSize, SWP_NOZORDER | SWP_NOACTIVATE);
        left += kSourceButtonSize + kSourceButtonGap;
    }
}

void AddNewDiskMediaDialog::UpdateArchiveOptions() {
    const auto enabled = ::IsDlgButtonChecked(m_hWnd, IDC_MEDIA_BROWSE_ARCHIVES) == BST_CHECKED;
    for (const auto control : kArchiveChildControls) {
        ::EnableWindow(::GetDlgItem(m_hWnd, control), enabled);
    }
}

void AddNewDiskMediaDialog::ClearSelection() {
    result_ = {};
    ::CheckRadioButton(m_hWnd, IDC_MEDIA_DRIVE_1, IDC_MEDIA_NETWORK, 0);
    ::SetDlgItemTextW(m_hWnd, IDC_MEDIA_NAME, L"");
    ::SetDlgItemTextW(m_hWnd, IDC_MEDIA_STATUS, L"No drive selected");
    UpdateConfirmEnabled();
}

void AddNewDiskMediaDialog::UpdateConfirmEnabled() {
    const auto selection = ::SendDlgItemMessageW(m_hWnd, IDC_MEDIA_CATALOG, CB_GETCURSEL, 0, 0);
    const bool hasCatalog = selection != CB_ERR;
    const bool hasSource = result_.kind != MediaSourceKind::None && IsDirectory(result_.scanRoot);
    ::EnableWindow(::GetDlgItem(m_hWnd, IDOK), hasCatalog && hasSource);
}

bool AddNewDiskMediaDialog::ApplySource(MediaSourceKind kind, const std::wstring& originalPath,
    const std::wstring& scanRoot, const std::wstring& status) {
    if (!IsDirectory(scanRoot)) {
        ClearSelection();
        return false;
    }
    result_.kind = kind;
    result_.originalPath = originalPath;
    result_.scanRoot = scanRoot;
    ::SetDlgItemTextW(m_hWnd, IDC_MEDIA_NAME, SourceName(originalPath).c_str());
    ::SetDlgItemTextW(m_hWnd, IDC_MEDIA_STATUS, status.c_str());
    UpdateConfirmEnabled();
    return true;
}

void AddNewDiskMediaDialog::SelectDrive(int index) {
    if (index < 0 || index >= static_cast<int>(drives_.size())) return;
    const auto& root = drives_[static_cast<std::size_t>(index)];
    ::CheckRadioButton(m_hWnd, IDC_MEDIA_DRIVE_1, IDC_MEDIA_NETWORK, IDC_MEDIA_DRIVE_1 + index);
    if (!ApplySource(MediaSourceKind::Drive, root, root, L"Selected drive: " + root)) {
        ::MessageBoxW(m_hWnd, L"The selected drive is not accessible.", L"Add New Disk/Media",
            MB_OK | MB_ICONWARNING);
    }
}

void AddNewDiskMediaDialog::SelectFolder() {
    std::wstring path;
    if (!ChooseFolder(m_hWnd, path)) return;
    ::CheckRadioButton(m_hWnd, IDC_MEDIA_DRIVE_1, IDC_MEDIA_NETWORK, IDC_MEDIA_NETWORK);
    if (!ApplySource(MediaSourceKind::Folder, path, path, L"Selected source: " + path)) {
        ::MessageBoxW(m_hWnd, L"The selected folder is not accessible.", L"Add New Disk/Media",
            MB_OK | MB_ICONWARNING);
    }
}

void AddNewDiskMediaDialog::SelectIso() {
    std::wstring path;
    if (!ChooseIso(m_hWnd, path)) return;
    ClearSelection();
    ::CheckRadioButton(m_hWnd, IDC_MEDIA_DRIVE_1, IDC_MEDIA_NETWORK, IDC_MEDIA_ISO);
    ::SetDlgItemTextW(m_hWnd, IDC_MEDIA_STATUS, L"Mounting selected ISO image...");
    ::UpdateWindow(m_hWnd);
    std::wstring mountedRoot;
    if (!MountIso(path, mountedRoot) ||
        !ApplySource(MediaSourceKind::Iso, path, mountedRoot, L"Selected ISO mounted at: " + mountedRoot)) {
        ::MessageBoxW(m_hWnd,
            L"Windows could not mount the selected ISO as a readable media source.",
            L"Add New Disk/Media", MB_OK | MB_ICONWARNING);
    }
}

bool AddNewDiskMediaDialog::Confirm() {
    if (result_.kind == MediaSourceKind::None || !IsDirectory(result_.scanRoot)) {
        ClearSelection();
        return false;
    }
    const auto diskNumber = ReadText(m_hWnd, IDC_MEDIA_NUMBER);
    if (!IsBlank(diskNumber)) {
        wchar_t* end{};
        errno = 0;
        const auto value = std::wcstoll(diskNumber.c_str(), &end, 10);
        if (errno == ERANGE || end == diskNumber.c_str() || *end != L'\0' || value < 0) {
            ::MessageBoxW(m_hWnd, L"Disk number must be a non-negative whole number.",
                L"Add New Disk/Media", MB_OK | MB_ICONWARNING);
            ::SetFocus(::GetDlgItem(m_hWnd, IDC_MEDIA_NUMBER));
            return false;
        }
        result_.diskNumber = std::to_wstring(value);
    } else {
        result_.diskNumber = L"0";
    }
    result_.diskName = ReadText(m_hWnd, IDC_MEDIA_NAME);
    result_.calculateCrc = ::IsDlgButtonChecked(m_hWnd, IDC_MEDIA_CALCULATE_CRC) == BST_CHECKED;
    result_.browseArchives = ::IsDlgButtonChecked(m_hWnd, IDC_MEDIA_BROWSE_ARCHIVES) == BST_CHECKED;
    if (IsBlank(result_.diskName)) result_.diskName = SourceName(result_.originalPath);
    const auto selected = ::SendDlgItemMessageW(m_hWnd, IDC_MEDIA_CATALOG, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR) return false;
    const auto index = static_cast<std::size_t>(::SendDlgItemMessageW(m_hWnd, IDC_MEDIA_CATALOG,
        CB_GETITEMDATA, selected, 0));
    if (index >= catalogs_.size()) return false;
    result_.destinationCatalogId = catalogs_[index].id;
    return true;
}
}
