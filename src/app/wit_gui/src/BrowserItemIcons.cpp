#include "wit_gui/BrowserItemIcons.h"
#include "resource.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cwctype>
#include <iterator>
#include <string_view>
#include <wincodec.h>

namespace wit::ui {
namespace {
constexpr int kIconSize = 16;

template<typename T>
void ReleaseIfPresent(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

HBITMAP LoadPngBitmap(IWICImagingFactory* factory, UINT resourceId) {
    const auto instance = GetModuleHandleW(nullptr);
    const auto resource = FindResourceW(instance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) return nullptr;
    const auto resourceData = LoadResource(instance, resource);
    const auto size = SizeofResource(instance, resource);
    const auto bytes = resourceData ? LockResource(resourceData) : nullptr;
    if (!bytes || size == 0) return nullptr;

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return nullptr;
    auto* destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(destination, bytes, size);
    GlobalUnlock(memory);

    IStream* stream{};
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return nullptr;
    }
    IWICBitmapDecoder* decoder{};
    IWICBitmapFrameDecode* frame{};
    IWICFormatConverter* converter{};
    HBITMAP bitmap{};
    void* pixels{};
    if (SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame)) &&
        SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = kIconSize;
        info.bmiHeader.biHeight = -kIconSize;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap || FAILED(converter->CopyPixels(nullptr, kIconSize * 4,
            kIconSize * kIconSize * 4, static_cast<BYTE*>(pixels)))) {
            if (bitmap) DeleteObject(bitmap);
            bitmap = nullptr;
        }
    }
    ReleaseIfPresent(converter);
    ReleaseIfPresent(frame);
    ReleaseIfPresent(decoder);
    ReleaseIfPresent(stream);
    return bitmap;
}

bool ExtensionEquals(std::wstring_view extension, std::wstring_view candidate) {
    return std::ranges::equal(extension, candidate, [](wchar_t left, wchar_t right) {
        return std::towlower(left) == std::towlower(right);
    });
}

int ImageForExtension(std::wstring_view extension) {
    constexpr std::array<std::wstring_view, 100> extensions{
        L"txt", L"doc", L"docx", L"rtf", L"pdf", L"odt", L"xls", L"xlsx", L"csv", L"ppt",
        L"pptx", L"pps", L"ppsx", L"mdb", L"accdb", L"jpg", L"jpeg", L"png", L"gif", L"bmp",
        L"tif", L"tiff", L"webp", L"svg", L"ico", L"heic", L"raw", L"psd", L"ai", L"eps",
        L"mp3", L"wav", L"wma", L"aac", L"flac", L"ogg", L"m4a", L"mid", L"midi", L"aiff",
        L"mp4", L"avi", L"mkv", L"mov", L"wmv", L"flv", L"webm", L"mpeg", L"mpg", L"m4v",
        L"zip", L"rar", L"7z", L"tar", L"gz", L"bz2", L"xz", L"iso", L"cab", L"dmg",
        L"exe", L"msi", L"bat", L"cmd", L"com", L"scr", L"dll", L"sys", L"drv", L"ocx",
        L"ini", L"cfg", L"conf", L"log", L"tmp", L"bak", L"dat", L"db", L"sqlite", L"reg",
        L"html", L"htm", L"css", L"js", L"json", L"xml", L"yaml", L"yml", L"php", L"asp",
        L"py", L"java", L"class", L"c", L"cpp", L"h", L"cs", L"rb", L"go", L"sh"
    };
    const auto found = std::ranges::find_if(extensions, [extension](std::wstring_view candidate) {
        return ExtensionEquals(extension, candidate);
    });
    return found == extensions.end() ? I_IMAGENONE
        : BrowserFileTxtImage + static_cast<int>(std::distance(extensions.begin(), found));
}

bool IsArchiveExtension(std::wstring_view extension) {
    constexpr std::array<std::wstring_view, 12> extensions{
        L"zip", L"7z", L"rar", L"tar", L"tgz", L"gz", L"bz2", L"xz", L"cab", L"arj", L"lha", L"iso"
    };
    return std::ranges::any_of(extensions, [extension](std::wstring_view candidate) {
        return ExtensionEquals(extension, candidate);
    });
}
}

HIMAGELIST CreateBrowserItemImageList() {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninitializeCom = SUCCEEDED(comResult);
    auto images = ImageList_Create(kIconSize, kIconSize, ILC_COLOR32, 105, 0);
    if (!images) {
        if (uninitializeCom) CoUninitialize();
        return nullptr;
    }
    IWICImagingFactory* factory{};
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)))) {
        ImageList_Destroy(images);
        if (uninitializeCom) CoUninitialize();
        return nullptr;
    }
    const auto addImage = [&](UINT id) {
        const auto bitmap = LoadPngBitmap(factory, id);
        const bool added = bitmap && ImageList_Add(images, bitmap, nullptr) != -1;
        if (bitmap) DeleteObject(bitmap);
        return added;
    };
    constexpr std::array<UINT, 5> baseImageIds{
        IDB_BROWSER_FOLDER, IDB_BROWSER_DOCUMENT, IDB_BROWSER_ARCHIVE,
        IDB_BROWSER_DATABASE, IDB_BROWSER_DRIVE
    };
    bool success = std::ranges::all_of(baseImageIds, addImage);
    for (UINT id = IDB_BROWSER_FILE_TXT; success && id <= IDB_BROWSER_FILE_SH; ++id) success = addImage(id);
    factory->Release();
    if (uninitializeCom) CoUninitialize();
    if (!success) {
        ImageList_Destroy(images);
        return nullptr;
    }
    return images;
}

int ImageForBrowserEntry(const wit::core::FileEntry& entry) {
    if (entry.isDirectory) return entry.isArchive ? BrowserArchiveImage : BrowserFolderImage;
    const int image = ImageForExtension(entry.extension);
    if (image != I_IMAGENONE) return image;
    return IsArchiveExtension(entry.extension) ? BrowserArchiveImage : BrowserDocumentImage;
}
}