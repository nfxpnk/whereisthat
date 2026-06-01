#include "wit_gui/FileListPane.h"
#include "wit_gui/BrowserItemIcons.h"
#include "wit_infra/StringUtils.h"
#include <wit_infra/Win32Helpers.h>
#include <CommCtrl.h>
#include <algorithm>
#include <array>
#include <cwctype>

namespace wit::ui {
namespace {
struct FileExtensionImage {
    const wchar_t* extension;
    int image;
};

const wchar_t* DiskTypeLabel(wit::core::DiskType type) {
    switch (type) {
    case wit::core::DiskType::CD: return L"CD";
    case wit::core::DiskType::DVD: return L"DVD";
    case wit::core::DiskType::BluRay: return L"BluRay";
    case wit::core::DiskType::HardDisk: return L"HardDisk";
    case wit::core::DiskType::SolidStateDisk: return L"SolidStateDisk";
    case wit::core::DiskType::RemovableUSB: return L"RemovableUSB";
    case wit::core::DiskType::VirtualDisk: return L"VirtualDisk";
    default: return L"Other";
    }
}

void InsertColumn(HWND hwnd, int index, const wchar_t* name, int width, int format = LVCFMT_LEFT) {
    LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH | LVCF_FMT};
    column.fmt = format;
    column.cx = width;
    column.pszText = const_cast<LPWSTR>(name);
    ListView_InsertColumn(hwnd, index, &column);
}

bool IsArchiveFileExtension(std::wstring extension) {
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
    constexpr std::array<const wchar_t*, 12> extensions{
        L"zip", L"7z", L"rar", L"tar", L"tgz", L"gz", L"bz2", L"xz", L"cab", L"arj", L"lha", L"iso"
    };
    return std::any_of(extensions.begin(), extensions.end(),
        [&extension](const wchar_t* candidate) { return extension == candidate; });
}

int ImageForFileExtension(const std::wstring& extension) {
    constexpr std::array<FileExtensionImage, 100> kFileExtensionImages{{
        { L"txt", BrowserFileTxtImage },
        { L"doc", BrowserFileDocImage },
        { L"docx", BrowserFileDocxImage },
        { L"rtf", BrowserFileRtfImage },
        { L"pdf", BrowserFilePdfImage },
        { L"odt", BrowserFileOdtImage },
        { L"xls", BrowserFileXlsImage },
        { L"xlsx", BrowserFileXlsxImage },
        { L"csv", BrowserFileCsvImage },
        { L"ppt", BrowserFilePptImage },
        { L"pptx", BrowserFilePptxImage },
        { L"pps", BrowserFilePpsImage },
        { L"ppsx", BrowserFilePpsxImage },
        { L"mdb", BrowserFileMdbImage },
        { L"accdb", BrowserFileAccdbImage },
        { L"jpg", BrowserFileJpgImage },
        { L"jpeg", BrowserFileJpegImage },
        { L"png", BrowserFilePngImage },
        { L"gif", BrowserFileGifImage },
        { L"bmp", BrowserFileBmpImage },
        { L"tif", BrowserFileTifImage },
        { L"tiff", BrowserFileTiffImage },
        { L"webp", BrowserFileWebpImage },
        { L"svg", BrowserFileSvgImage },
        { L"ico", BrowserFileIcoImage },
        { L"heic", BrowserFileHeicImage },
        { L"raw", BrowserFileRawImage },
        { L"psd", BrowserFilePsdImage },
        { L"ai", BrowserFileAiImage },
        { L"eps", BrowserFileEpsImage },
        { L"mp3", BrowserFileMp3Image },
        { L"wav", BrowserFileWavImage },
        { L"wma", BrowserFileWmaImage },
        { L"aac", BrowserFileAacImage },
        { L"flac", BrowserFileFlacImage },
        { L"ogg", BrowserFileOggImage },
        { L"m4a", BrowserFileM4aImage },
        { L"mid", BrowserFileMidImage },
        { L"midi", BrowserFileMidiImage },
        { L"aiff", BrowserFileAiffImage },
        { L"mp4", BrowserFileMp4Image },
        { L"avi", BrowserFileAviImage },
        { L"mkv", BrowserFileMkvImage },
        { L"mov", BrowserFileMovImage },
        { L"wmv", BrowserFileWmvImage },
        { L"flv", BrowserFileFlvImage },
        { L"webm", BrowserFileWebmImage },
        { L"mpeg", BrowserFileMpegImage },
        { L"mpg", BrowserFileMpgImage },
        { L"m4v", BrowserFileM4vImage },
        { L"zip", BrowserFileZipImage },
        { L"rar", BrowserFileRarImage },
        { L"7z", BrowserFile7ZImage },
        { L"tar", BrowserFileTarImage },
        { L"gz", BrowserFileGzImage },
        { L"bz2", BrowserFileBz2Image },
        { L"xz", BrowserFileXzImage },
        { L"iso", BrowserFileIsoImage },
        { L"cab", BrowserFileCabImage },
        { L"dmg", BrowserFileDmgImage },
        { L"exe", BrowserFileExeImage },
        { L"msi", BrowserFileMsiImage },
        { L"bat", BrowserFileBatImage },
        { L"cmd", BrowserFileCmdImage },
        { L"com", BrowserFileComImage },
        { L"scr", BrowserFileScrImage },
        { L"dll", BrowserFileDllImage },
        { L"sys", BrowserFileSysImage },
        { L"drv", BrowserFileDrvImage },
        { L"ocx", BrowserFileOcxImage },
        { L"ini", BrowserFileIniImage },
        { L"cfg", BrowserFileCfgImage },
        { L"conf", BrowserFileConfImage },
        { L"log", BrowserFileLogImage },
        { L"tmp", BrowserFileTmpImage },
        { L"bak", BrowserFileBakImage },
        { L"dat", BrowserFileDatImage },
        { L"db", BrowserFileDbImage },
        { L"sqlite", BrowserFileSqliteImage },
        { L"reg", BrowserFileRegImage },
        { L"html", BrowserFileHtmlImage },
        { L"htm", BrowserFileHtmImage },
        { L"css", BrowserFileCssImage },
        { L"js", BrowserFileJsImage },
        { L"json", BrowserFileJsonImage },
        { L"xml", BrowserFileXmlImage },
        { L"yaml", BrowserFileYamlImage },
        { L"yml", BrowserFileYmlImage },
        { L"php", BrowserFilePhpImage },
        { L"asp", BrowserFileAspImage },
        { L"py", BrowserFilePyImage },
        { L"java", BrowserFileJavaImage },
        { L"class", BrowserFileClassImage },
        { L"c", BrowserFileCImage },
        { L"cpp", BrowserFileCppImage },
        { L"h", BrowserFileHImage },
        { L"cs", BrowserFileCsImage },
        { L"rb", BrowserFileRbImage },
        { L"go", BrowserFileGoImage },
        { L"sh", BrowserFileShImage },
    }};

    const auto match = std::find_if(kFileExtensionImages.begin(), kFileExtensionImages.end(),
        [&extension](const FileExtensionImage& item) { return extension == item.extension; });
    return match != kFileExtensionImages.end() ? match->image : I_IMAGENONE;
}
}

void FileListView::ConfigureColumns() {
    while (ListView_DeleteColumn(hwnd, 0)) {}
    if (ShowsDisks()) {
        InsertColumn(hwnd, 0, L"Disk Name", 180);
        InsertColumn(hwnd, 1, L"Media Type", 100);
        InsertColumn(hwnd, 2, L"Capacity", 100, LVCFMT_RIGHT);
        InsertColumn(hwnd, 3, L"Free Space", 100, LVCFMT_RIGHT);
        InsertColumn(hwnd, 4, L"Last Updated", 165);
        InsertColumn(hwnd, 5, L"Disk #", 70, LVCFMT_RIGHT);
        InsertColumn(hwnd, 6, L"Description", 180);
        InsertColumn(hwnd, 7, L"Category", 120);
        InsertColumn(hwnd, 8, L"Disk Location", 240);
        return;
    }
    InsertColumn(hwnd, 0, L"Name", 200);
    InsertColumn(hwnd, 1, L"Type", 80);
    InsertColumn(hwnd, 2, L"Size", 100, LVCFMT_RIGHT);
    InsertColumn(hwnd, 3, L"Path", 320);
    InsertColumn(hwnd, 4, L"Modified", 180);
}

void FileListView::SetLocation(const wit::core::BrowserLocation& newLocation, wit::storage::Database* database) {
    location = newLocation;
    db = database;
    total = 0;
    pageStart = -1;
    page.clear();
    diskPage.clear();
    ListView_SetItemCountEx(hwnd, 0, LVSICF_NOINVALIDATEALL);
    ConfigureColumns();
    total = db ? (ShowsDisks() ? db->GetDiskCount() : db->GetBrowserItemCount(location)) : 0;
    ListView_SetItemCountEx(hwnd, total, LVSICF_NOINVALIDATEALL);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void FileListView::EnsurePage(int index) {
    if (!db) return;
    const int pageSize = 256;
    int desired = (index / pageSize) * pageSize;
    if (desired == pageStart) return;
    if (ShowsDisks()) {
        diskPage = db->GetDisksPage(desired, pageSize);
        page.clear();
    } else {
        page = db->GetBrowserItemsPage(location, desired, pageSize);
        diskPage.clear();
    }
    pageStart = desired;
}

const wit::core::FileEntry* FileListView::EntryAt(int row) {
    if (ShowsDisks()) return nullptr;
    EnsurePage(row);
    const int index = row - pageStart;
    return index >= 0 && index < static_cast<int>(page.size()) ? &page[index] : nullptr;
}

const wit::core::Disk* FileListView::DiskAt(int row) {
    if (!ShowsDisks()) return nullptr;
    EnsurePage(row);
    const int index = row - pageStart;
    return index >= 0 && index < static_cast<int>(diskPage.size()) ? &diskPage[index] : nullptr;
}

int FileListView::ImageFor(int row) {
    if (ShowsDisks()) return DiskAt(row) ? BrowserDriveImage : I_IMAGENONE;
    const auto* entry = EntryAt(row);
    if (!entry) return I_IMAGENONE;
    if (entry->isDirectory) return entry->isArchive ? BrowserArchiveImage : BrowserFolderImage;
    auto extension = entry->extension;
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    const int fileImage = ImageForFileExtension(extension);
    if (fileImage != I_IMAGENONE) return fileImage;
    if (IsArchiveFileExtension(extension)) return BrowserArchiveImage;
    return BrowserDocumentImage;
}

std::wstring FileListView::TextFor(int row, int column) {
    if (ShowsDisks()) {
        const auto* disk = DiskAt(row);
        if (!disk) return L"";
        switch (column) {
        case 0: return disk->diskName;
        case 1: return DiskTypeLabel(disk->diskType);
        case 2: return wit::core::FormatSize(disk->totalCapacity);
        case 3: return wit::core::FormatSize(disk->freeSpace);
        case 4: return wit::platform::FormatUnixTimestamp(disk->updatedAt);
        case 5: return std::to_wstring(disk->diskNumber);
        case 6: return disk->description;
        case 7: return disk->category;
        case 8: return disk->location;
        default: return L"";
        }
    }
    const auto* entry = EntryAt(row);
    if (!entry) return L"";
    const auto& file = *entry;
    switch (column) {
    case 0:
        return file.name;
    case 1:
        return file.isArchive ? L"Archive" : (file.isDirectory ? L"Folder" : file.extension);
    case 2:
        return wit::core::FormatSize(file.size);
    case 3:
        return file.parentPath;
    case 4:
        return file.modifiedAt;
    default:
        return L"";
    }
}
}
#include "wit_gui/FileListPane.h"
#include <CommCtrl.h>

namespace wit::ui {
void CatalogListView::Reload() {
    ListView_DeleteAllItems(hwnd);
    for (std::size_t index = 0; index < catalogs.size(); ++index) {
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(index);
        item.pszText = const_cast<LPWSTR>(catalogs[index].name.c_str());
        item.lParam = static_cast<LPARAM>(catalogs[index].id);
        ListView_InsertItem(hwnd, &item);
    }
}
}


