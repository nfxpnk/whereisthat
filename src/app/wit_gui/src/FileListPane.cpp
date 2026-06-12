#include "wit_gui/FileListPane.h"
#include "wit_gui/BrowserItemIcons.h"
#include "wit_infra/StringUtils.h"
#include <wit_infra/AppSettings.h>
#include <wit_infra/Win32Helpers.h>
#include <CommCtrl.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <format>
#include <iterator>
#include <strsafe.h>
#include <string_view>
#include <unordered_map>

namespace wit::ui {
namespace {
struct FileExtensionImage {
    const wchar_t* extension;
    int image;
};

struct ColumnDefinition {
    const wchar_t* key;
    const wchar_t* name;
    int defaultWidth;
    int format;
};

constexpr int kMinimumColumnWidth = 20;
constexpr int kMaximumColumnWidth = 4000;

constexpr std::array<ColumnDefinition, 9> kBrowserRootColumns{{
    { L"BrowserRoot.Name", L"Name", 180, LVCFMT_LEFT },
    { L"BrowserRoot.MediaType", L"Media Type", 100, LVCFMT_LEFT },
    { L"BrowserRoot.Capacity", L"Capacity", 100, LVCFMT_RIGHT },
    { L"BrowserRoot.FreeSpace", L"Free Space", 100, LVCFMT_RIGHT },
    { L"BrowserRoot.LastUpdated", L"Last Updated", 165, LVCFMT_LEFT },
    { L"BrowserRoot.DiskNumber", L"Disk # / Count", 95, LVCFMT_RIGHT },
    { L"BrowserRoot.Description", L"Description", 180, LVCFMT_LEFT },
    { L"BrowserRoot.Category", L"Category", 120, LVCFMT_LEFT },
    { L"BrowserRoot.DiskLocation", L"Disk Location", 240, LVCFMT_LEFT },
}};

constexpr std::array<ColumnDefinition, 5> kBrowserContentColumns{{
    { L"BrowserContent.Name", L"Name", 200, LVCFMT_LEFT },
    { L"BrowserContent.Type", L"Type", 80, LVCFMT_LEFT },
    { L"BrowserContent.Size", L"Size", 100, LVCFMT_RIGHT },
    { L"BrowserContent.Path", L"Path", 320, LVCFMT_LEFT },
    { L"BrowserContent.Modified", L"Modified", 180, LVCFMT_LEFT },
}};

const wchar_t* DiskTypeLabel(wit::core::DiskType type) {
    switch (type) {
    case wit::core::DiskType::CD: return L"CD/DVD";
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

bool IsValidColumnWidth(int width) {
    return width >= kMinimumColumnWidth && width <= kMaximumColumnWidth;
}

int WidthForColumn(const wit::platform::AppSettings& settings, const ColumnDefinition& column) {
    const auto saved = settings.fileListColumnWidths.find(column.key);
    if (saved == settings.fileListColumnWidths.end() || !IsValidColumnWidth(saved->second)) {
        return column.defaultWidth;
    }
    return saved->second;
}

template <std::size_t Size>
void InsertColumns(HWND hwnd, const std::array<ColumnDefinition, Size>& columns,
    const wit::platform::AppSettings& settings) {
    for (std::size_t index = 0; index < columns.size(); ++index) {
        const auto& column = columns[index];
        InsertColumn(hwnd, static_cast<int>(index), column.name, WidthForColumn(settings, column), column.format);
    }
}

bool FileExtensionEquals(std::wstring_view extension, std::wstring_view candidate) {
    return std::ranges::equal(extension, candidate,
        [](wchar_t left, wchar_t right) { return std::towlower(left) == std::towlower(right); });
}

bool IsArchiveFileExtension(std::wstring_view extension) {
    constexpr std::array<std::wstring_view, 12> extensions{
        L"zip", L"7z", L"rar", L"tar", L"tgz", L"gz", L"bz2", L"xz", L"cab", L"arj", L"lha", L"iso"
    };
    return std::ranges::any_of(extensions,
        [extension](std::wstring_view candidate) { return FileExtensionEquals(extension, candidate); });
}

int ImageForFileExtension(std::wstring_view extension) {
    static const std::unordered_map<std::wstring, int> kFileExtensionImages = [] {
        constexpr std::array<FileExtensionImage, 100> images{{
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
        std::unordered_map<std::wstring, int> map;
        map.reserve(images.size());
        for (const auto& image : images) {
            map.emplace(image.extension, image.image);
        }
        return map;
    }();

    std::wstring key(extension);
    std::ranges::transform(key, key.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    const auto match = kFileExtensionImages.find(key);
    return match != kFileExtensionImages.end() ? match->second : I_IMAGENONE;
}

void CopyText(std::wstring_view text, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    StringCchCopyNW(buffer, bufferSize, text.data(), text.size());
}
}

void FileListView::ConfigureColumns() {
    while (ListView_DeleteColumn(hwnd, 0)) {}
    const auto settings = wit::platform::LoadAppSettings();
    if (ShowsBrowserItems()) {
        InsertColumns(hwnd, kBrowserRootColumns, settings);
        return;
    }
    InsertColumns(hwnd, kBrowserContentColumns, settings);
}

bool FileListView::PersistColumnWidths() const {
    if (!hwnd) return false;

    auto settings = wit::platform::LoadAppSettings();
    const auto persist = [&](const auto& columns) {
        for (std::size_t index = 0; index < columns.size(); ++index) {
            const int width = ListView_GetColumnWidth(hwnd, static_cast<int>(index));
            if (IsValidColumnWidth(width)) settings.fileListColumnWidths[columns[index].key] = width;
        }
    };
    if (ShowsBrowserItems()) {
        persist(kBrowserRootColumns);
    } else {
        persist(kBrowserContentColumns);
    }
    return wit::platform::SaveAppSettings(settings);
}

void FileListView::SetLocation(
    const wit::core::BrowserLocation& newLocation, wit::storage::IBrowserRepository* repository) {
    location = newLocation;
    browser = repository;
    total = 0;
    browserPageStart = -1;
    browserPage.clear();
    ClearCache();
    ListView_SetItemCountEx(hwnd, 0, LVSICF_NOINVALIDATEALL);
    ConfigureColumns();
    total = browser ? (ShowsBrowserItems() ? browser->GetBrowserRootItemCount(location)
        : browser->GetBrowserItemCount(location)) : 0;
    ListView_SetItemCountEx(hwnd, total, LVSICF_NOINVALIDATEALL);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void FileListView::ResetCachedItems() {
    browserPageStart = -1;
    browserPage.clear();
    ClearCache();
    if (!hwnd) return;
    ListView_SetItemCountEx(hwnd, total, LVSICF_NOSCROLL);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void FileListView::PreloadRange(int firstRow, int lastRow) {
    if (!browser || ShowsBrowserItems() || total <= 0) return;
    firstRow = std::clamp(firstRow, 0, total - 1);
    lastRow = std::clamp(lastRow, firstRow, total - 1);

    const int firstPage = (std::max)(0, (firstRow / PageSize) - 1);
    const int lastPage = (std::min)((total - 1) / PageSize, (lastRow / PageSize) + 1);
    for (int pageIndex = firstPage; pageIndex <= lastPage; ++pageIndex) {
        CacheFilePage(pageIndex * PageSize);
    }
}

void FileListView::ClearCache() {
    cacheClock_ = 0;
    cachedFilePages_.clear();
}

void FileListView::CacheFilePage(int pageStartValue) {
    if (!browser || ShowsBrowserItems() || pageStartValue < 0 || pageStartValue >= total) return;

    const int normalizedStart = (pageStartValue / PageSize) * PageSize;
    const auto found = std::ranges::find_if(cachedFilePages_,
        [normalizedStart](const CachedFilePage& cachedPage) { return cachedPage.start == normalizedStart; });
    if (found != cachedFilePages_.end()) {
        found->lastUsed = ++cacheClock_;
        return;
    }

    CachedFilePage cachedPage;
    cachedPage.start = normalizedStart;
    cachedPage.items = browser->GetBrowserItemsPage(location, normalizedStart, PageSize);
    cachedPage.lastUsed = ++cacheClock_;
    cachedFilePages_.push_back(std::move(cachedPage));

    while (cachedFilePages_.size() > MaxCachedPages) {
        const auto oldest = std::ranges::min_element(cachedFilePages_,
            [](const CachedFilePage& left, const CachedFilePage& right) { return left.lastUsed < right.lastUsed; });
        if (oldest == cachedFilePages_.end()) break;
        cachedFilePages_.erase(oldest);
    }
}

const wit::core::FileEntry* FileListView::EntryAt(int row) {
    if (!browser || ShowsBrowserItems() || row < 0 || row >= total) return nullptr;
    const int pageStart = (row / PageSize) * PageSize;
    CacheFilePage(pageStart);
    const auto found = std::ranges::find_if(cachedFilePages_,
        [pageStart](const CachedFilePage& cachedPage) { return cachedPage.start == pageStart; });
    if (found == cachedFilePages_.end()) return nullptr;
    found->lastUsed = ++cacheClock_;
    const int index = row - found->start;
    return index >= 0 && index < static_cast<int>(found->items.size()) ? &found->items[index] : nullptr;
}

const wit::core::Disk* FileListView::DiskAt(int row) {
    const auto* item = BrowserItemAt(row);
    return item && item->type == wit::core::BrowserItemType::Disk ? &item->disk : nullptr;
}

const wit::core::BrowserItem* FileListView::BrowserItemAt(int row) {
    if (!browser || !ShowsBrowserItems() || row < 0 || row >= total) return nullptr;
    const int pageStart = (row / PageSize) * PageSize;
    if (browserPageStart != pageStart) {
        browserPage = browser->GetBrowserRootItemsPage(location, pageStart, PageSize);
        browserPageStart = pageStart;
    }
    const int index = row - pageStart;
    return index >= 0 && index < static_cast<int>(browserPage.size()) ? &browserPage[index] : nullptr;
}

bool FileListView::SelectEntry(std::int64_t id, bool isDirectory) {
    if (!hwnd || ShowsBrowserItems()) return false;
    for (int row = 0; row < total; ++row) {
        const auto* entry = EntryAt(row);
        if (!entry || entry->id != id || entry->isDirectory != isDirectory) continue;
        ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_SetItemState(hwnd, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(hwnd, row, FALSE);
        return true;
    }
    return false;
}

int FileListView::ImageFor(int row) {
    if (ShowsBrowserItems()) {
        const auto* item = BrowserItemAt(row);
        if (!item) return I_IMAGENONE;
        return item->type == wit::core::BrowserItemType::DiskGroup ? BrowserFolderImage : BrowserDriveImage;
    }
    const auto* entry = EntryAt(row);
    if (!entry) return I_IMAGENONE;
    if (entry->isDirectory) return entry->isArchive ? BrowserArchiveImage : BrowserFolderImage;
    const std::wstring_view extension = entry->extension;
    const int fileImage = ImageForFileExtension(extension);
    if (fileImage != I_IMAGENONE) return fileImage;
    if (IsArchiveFileExtension(extension)) return BrowserArchiveImage;
    return BrowserDocumentImage;
}

void FileListView::TextFor(int row, int column, wchar_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    buffer[0] = L'\0';
    if (ShowsDisks()) {
        const auto* item = BrowserItemAt(row);
        if (!item) return;
        if (item->type == wit::core::BrowserItemType::DiskGroup) {
            switch (column) {
            case 0: CopyText(item->group.name, buffer, bufferSize); return;
            case 1: CopyText(L"Disk Group", buffer, bufferSize); return;
            case 2: wit::core::FormatSizeToBuffer(item->group.totalCapacity, buffer, bufferSize); return;
            case 3: wit::core::FormatSizeToBuffer(item->group.freeSpace, buffer, bufferSize); return;
            case 4: wit::platform::FormatUnixTimestampToBuffer(item->group.updatedAt, buffer, bufferSize); return;
            case 5: swprintf_s(buffer, bufferSize, L"%lld", static_cast<long long>(item->group.totalDisks)); return;
            default: return;
            }
        }
        const auto* disk = &item->disk;
        switch (column) {
        case 0: CopyText(disk->diskName, buffer, bufferSize); return;
        case 1: CopyText(DiskTypeLabel(disk->diskType), buffer, bufferSize); return;
        case 2: wit::core::FormatSizeToBuffer(disk->totalCapacity, buffer, bufferSize); return;
        case 3: wit::core::FormatSizeToBuffer(disk->freeSpace, buffer, bufferSize); return;
        case 4: wit::platform::FormatUnixTimestampToBuffer(disk->updatedAt, buffer, bufferSize); return;
        case 5: swprintf_s(buffer, bufferSize, L"%lld", static_cast<long long>(disk->diskNumber)); return;
        case 6: CopyText(disk->description, buffer, bufferSize); return;
        case 7: CopyText(disk->category, buffer, bufferSize); return;
        case 8: CopyText(disk->location, buffer, bufferSize); return;
        default: return;
        }
    }
    const auto* entry = EntryAt(row);
    if (!entry) return;
    const auto& file = *entry;
    switch (column) {
    case 0:
        CopyText(file.name, buffer, bufferSize);
        return;
    case 1:
        CopyText(file.isArchive ? std::wstring_view(L"Archive") :
            (file.isDirectory ? std::wstring_view(L"Folder") : std::wstring_view(file.extension)), buffer, bufferSize);
        return;
    case 2:
        wit::core::FormatSizeToBuffer(file.size, buffer, bufferSize);
        return;
    case 3:
        CopyText(file.parentPath, buffer, bufferSize);
        return;
    case 4:
        wit::platform::FormatUnixTimestampToBuffer(file.modifiedAt, buffer, bufferSize);
        return;
    default:
        return;
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


