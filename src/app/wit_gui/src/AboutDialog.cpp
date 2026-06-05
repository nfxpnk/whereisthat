#include "wit_gui/AboutDialog.h"

#include "wit_infra/StringUtils.h"
#include "wit_infra/Win32Helpers.h"

#include <shellapi.h>

#include <string>
#include <string_view>

namespace {

constexpr wchar_t kProjectUrl[] = L"https://nfxpnk.github.io/whereisthat";
constexpr wchar_t kRepositoryUrl[] = L"https://github.com/nfxpnk/whereisthat";
constexpr wchar_t kUnknownVersionText[] = L"Version: unknown (64-bit)";

HFONT CreateBoldDialogFont(HWND dialog, int pointSizeDelta) {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        return nullptr;
    }

    LOGFONTW font = metrics.lfMessageFont;
    font.lfWeight = FW_BOLD;
    font.lfHeight -= pointSizeDelta;
    return CreateFontIndirectW(&font);
}

std::wstring LoadAboutVersionText() {
    const HMODULE module = GetModuleHandleW(nullptr);
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_APP_VERSION), RT_RCDATA);
    if (resource == nullptr) {
        return kUnknownVersionText;
    }

    const HGLOBAL handle = LoadResource(module, resource);
    if (handle == nullptr) {
        return kUnknownVersionText;
    }

    const DWORD size = SizeofResource(module, resource);
    const void* data = LockResource(handle);
    if (data == nullptr || size == 0) {
        return kUnknownVersionText;
    }

    const auto* bytes = static_cast<const char*>(data);
    const std::string_view version = wit::core::TrimAsciiWhitespace(std::string_view(bytes, size));
    const std::wstring versionText = wit::platform::ToUtf16(std::string(version));
    if (versionText.empty()) {
        return kUnknownVersionText;
    }

    return L"Version: " + versionText + L" (64-bit)";
}

}

namespace wit::ui {

bool AboutDialog::Show(HWND owner) {
    return DoModal(owner) == IDOK;
}

LRESULT AboutDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    CenterWindow(GetParent());

    auto tabs = GetDlgItem(IDC_ABOUT_TABS);
    TCITEMW infoTab{};
    infoTab.mask = TCIF_TEXT;
    infoTab.pszText = const_cast<LPWSTR>(L"Info");
    TabCtrl_InsertItem(tabs, 0, &infoTab);

    TCITEMW licenseTab{};
    licenseTab.mask = TCIF_TEXT;
    licenseTab.pszText = const_cast<LPWSTR>(L"License");
    TabCtrl_InsertItem(tabs, 1, &licenseTab);
    TabCtrl_SetCurSel(tabs, 0);

    icon_ = reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR));
    SendDlgItemMessageW(IDC_ABOUT_ICON, STM_SETICON, reinterpret_cast<WPARAM>(icon_), 0);

    titleFont_ = CreateBoldDialogFont(m_hWnd, 0);
    versionFont_ = CreateBoldDialogFont(m_hWnd, 0);
    if (titleFont_ != nullptr) {
        SendDlgItemMessageW(IDC_ABOUT_TITLE, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
    }
    if (versionFont_ != nullptr) {
        SendDlgItemMessageW(IDC_ABOUT_VERSION, WM_SETFONT, reinterpret_cast<WPARAM>(versionFont_), TRUE);
    }
    SetDlgItemTextW(IDC_ABOUT_VERSION, LoadAboutVersionText().c_str());

    ShowActivePage();
    return TRUE;
}

LRESULT AboutDialog::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    if (titleFont_ != nullptr) {
        DeleteObject(titleFont_);
        titleFont_ = nullptr;
    }
    if (versionFont_ != nullptr) {
        DeleteObject(versionFont_);
        versionFont_ = nullptr;
    }
    if (icon_ != nullptr) {
        DestroyIcon(icon_);
        icon_ = nullptr;
    }
    return 0;
}

LRESULT AboutDialog::OnTabChanged(int, LPNMHDR, BOOL&) {
    ShowActivePage();
    return 0;
}

LRESULT AboutDialog::OnLinkActivated(int, LPNMHDR, BOOL&) {
    ShellExecuteW(m_hWnd, L"open", kProjectUrl, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

LRESULT AboutDialog::OnRepositoryLinkActivated(int, LPNMHDR, BOOL&) {
    ShellExecuteW(m_hWnd, L"open", kRepositoryUrl, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

LRESULT AboutDialog::OnClose(WORD, WORD id, HWND, BOOL&) {
    EndDialog(id);
    return 0;
}

void AboutDialog::ShowActivePage() {
    const int selected = TabCtrl_GetCurSel(GetDlgItem(IDC_ABOUT_TABS));
    const bool showInfo = selected == 0;

    GetDlgItem(IDC_ABOUT_PAGE_INFO).ShowWindow(showInfo ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_ABOUT_ICON).ShowWindow(showInfo ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_ABOUT_TITLE).ShowWindow(showInfo ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_ABOUT_COPYRIGHT).ShowWindow(showInfo ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_ABOUT_VERSION).ShowWindow(showInfo ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_ABOUT_LINK_LABEL).ShowWindow(showInfo ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_ABOUT_LINK).ShowWindow(showInfo ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_ABOUT_REPOSITORY_LINK).ShowWindow(showInfo ? SW_SHOW : SW_HIDE);
    GetDlgItem(IDC_ABOUT_PAGE_LICENSE).ShowWindow(showInfo ? SW_HIDE : SW_SHOW);
}

}

