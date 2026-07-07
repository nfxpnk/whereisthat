#include <gtest/gtest.h>
#include <wit_database/Database.h>
#include <wit_infra/AppSettings.h>
#include <wit_infra/PathHelpers.h>
#include <wit_types/FolderEntry.h>
#include <wit_gui/BrowserItemIcons.h>
#include <wit_gui/FileListPane.h>
#include <wit_gui/SearchPane.h>
#include <wit_gui/TreeViewPane.h>
#include "wit_gui/OwnerDataPageCache.h"
#include <CommCtrl.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

WTL::CAppModule _Module;

namespace {
class AppSettingsGuard {
public:
    AppSettingsGuard() : original_(wit::platform::LoadAppSettings()) {}
    ~AppSettingsGuard() { (void)wit::platform::SaveAppSettings(original_); }

private:
    wit::platform::AppSettings original_;
};
class AtlModuleGuard {
public:
    AtlModuleGuard() {
        initialized_ = SUCCEEDED(_Module.Init(nullptr, GetModuleHandleW(nullptr)));
    }


    bool initialized() const { return initialized_; }

private:
    bool initialized_{};
};

void PumpMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

class BlockingSearchRepository final : public wit::search::ISearchRepository {
public:
    wit::search::PreparedSearchResult PrepareByName(
        const std::wstring& nameTerm, int limit, wit::core::FileSort sort, bool caseSensitive) override {
        wit::search::PreparedSearchResult result;
        result.total = CountByName(nameTerm, caseSensitive);
        if (result.total > 0) result.entries = PageByName(nameTerm, 0, limit, sort, caseSensitive);
        return result;
    }

    wit::search::PreparedSearchResult PrepareAdvanced(
        const wit::search::AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort) override {
        wit::search::PreparedSearchResult result;
        result.total = CountAdvanced(expression);
        if (result.total > 0) result.entries = PageAdvanced(expression, 0, limit, sort);
        return result;
    }

    int CountByName(const std::wstring&, bool) override {
        countStarted = true;
        while (!cancelled.load()) std::this_thread::yield();
        return 0;
    }

    std::vector<wit::core::FileEntry> PageByName(
        const std::wstring&, int, int, wit::core::FileSort, bool) override {
        return {};
    }

    int CountAdvanced(const wit::search::AdvancedSearchExpression&) override {
        return CountByName({}, false);
    }

    std::vector<wit::core::FileEntry> PageAdvanced(
        const wit::search::AdvancedSearchExpression&, int, int, wit::core::FileSort) override {
        return {};
    }

    void CancelPending() override {
        cancelled = true;
        ++cancelCalls;
    }

    std::wstring LastErrorMessage() const override { return {}; }

    std::atomic_bool countStarted{};
    std::atomic_bool cancelled{};
    std::atomic_int cancelCalls{};
};

class ImmediateSearchRepository final : public wit::search::ISearchRepository {
public:
    wit::search::PreparedSearchResult PrepareByName(
        const std::wstring& nameTerm, int limit, wit::core::FileSort sort, bool caseSensitive) override {
        wit::search::PreparedSearchResult result;
        result.total = CountByName(nameTerm, caseSensitive);
        if (result.total > 0) result.entries = PageByName(nameTerm, 0, limit, sort, caseSensitive);
        return result;
    }

    wit::search::PreparedSearchResult PrepareAdvanced(
        const wit::search::AdvancedSearchExpression& expression, int limit, wit::core::FileSort sort) override {
        wit::search::PreparedSearchResult result;
        result.total = CountAdvanced(expression);
        if (result.total > 0) result.entries = PageAdvanced(expression, 0, limit, sort);
        return result;
    }

    int CountByName(const std::wstring&, bool) override { return 1; }

    std::vector<wit::core::FileEntry> PageByName(
        const std::wstring&, int, int, wit::core::FileSort, bool) override {
        wit::core::FileEntry entry;
        entry.name = L"replacement.txt";
        entry.extension = L"txt";
        entry.size = 700ull * 1024ull * 1024ull;
        entry.modifiedAt = 1700000000;
        return {std::move(entry)};
    }

    int CountAdvanced(const wit::search::AdvancedSearchExpression&) override { return 1; }

    std::vector<wit::core::FileEntry> PageAdvanced(
        const wit::search::AdvancedSearchExpression&, int, int, wit::core::FileSort) override {
        return PageByName({}, 0, 1, {}, false);
    }

    void CancelPending() override {}
    std::wstring LastErrorMessage() const override { return {}; }
};

class BlockingBrowserRepository final : public wit::storage::IBrowserRepository {
public:
    int GetBrowserItemCount(const wit::core::BrowserLocation&) override {
        countStarted = true;
        while (!releaseCount.load()) Sleep(1);
        return 1;
    }
    int GetBrowserRootItemCount(const wit::core::BrowserLocation&) override { return 0; }

    std::vector<wit::core::BrowserItem> GetBrowserRootItemsPage(
        const wit::core::BrowserLocation&, int, int, wit::core::BrowserRootSort) override {
        return {};
    }

    std::vector<wit::core::FileEntry> GetBrowserItemsPage(
        const wit::core::BrowserLocation&, int, int, wit::core::FileSort) override {
        wit::core::FileEntry entry;
        entry.id = 1;
        entry.name = L"loaded.txt";
        entry.extension = L"txt";
        return {std::move(entry)};
    }

    bool HasChildFolders(std::int64_t, const std::wstring&) override { return false; }

    std::vector<wit::core::FileEntry> GetChildFolders(
        std::int64_t, const std::wstring&) override {
        return {};
    }

    std::wstring LastErrorMessage() const override { return {}; }

    std::atomic_bool countStarted{};
    std::atomic_bool releaseCount{};
};

class CountingBrowserRepository final : public wit::storage::IBrowserRepository {
public:
    int GetBrowserItemCount(const wit::core::BrowserLocation&) override { return 500000; }
    int GetBrowserRootItemCount(const wit::core::BrowserLocation&) override { return 0; }

    std::vector<wit::core::BrowserItem> GetBrowserRootItemsPage(
        const wit::core::BrowserLocation&, int, int, wit::core::BrowserRootSort) override {
        return {};
    }

    std::vector<wit::core::FileEntry> GetBrowserItemsPage(
        const wit::core::BrowserLocation&, int offset, int limit, wit::core::FileSort) override {
        ++pageCalls;
        std::vector<wit::core::FileEntry> entries;
        entries.reserve(static_cast<std::size_t>(limit));
        for (int index = 0; index < limit; ++index) {
            wit::core::FileEntry entry;
            entry.id = offset + index + 1;
            entry.name = L"file" + std::to_wstring(offset + index) + L".txt";
            entry.extension = L"txt";
            entries.push_back(std::move(entry));
        }
        return entries;
    }

    bool HasChildFolders(std::int64_t, const std::wstring&) override { return false; }

    std::vector<wit::core::FileEntry> GetChildFolders(
        std::int64_t, const std::wstring&) override {
        return {};
    }

    std::wstring LastErrorMessage() const override { return {}; }

    int pageCalls{};
};
std::wstring StatusPartText(HWND status, int part) {
    const auto length = LOWORD(SendMessageW(status, SB_GETTEXTLENGTHW, part, 0));
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    SendMessageW(status, SB_GETTEXTW, part, reinterpret_cast<LPARAM>(text.data()));
    text.resize(length);
    return text;
}

std::wstring WindowText(HWND window) {
    const int length = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

DWORD SelectionStart(HWND window) {
    DWORD start{};
    DWORD end{};
    SendMessageW(window, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    return start;
}

template <typename Func>
double MeasureMilliseconds(Func&& func) {
    const auto started = std::chrono::steady_clock::now();
    func();
    PumpMessages();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

int TreeChildCount(HWND tree, HTREEITEM parent) {
    int count{};
    for (auto child = TreeView_GetChild(tree, parent); child; child = TreeView_GetNextSibling(tree, child)) ++count;
    return count;
}

HTREEITEM FindDisplayedDiskGroup(wit::ui::CatalogTreeView& catalogTree, HWND tree, HTREEITEM parent,
    std::int64_t diskGroupId) {
    for (auto child = TreeView_GetChild(tree, parent); child; child = TreeView_GetNextSibling(tree, child)) {
        const auto* target = catalogTree.TargetFor(child);
        if (target && target->location.isDiskGroup && target->location.diskGroupId == diskGroupId) return child;
    }
    return nullptr;
}

wit::core::Disk TestDisk(const std::wstring& name, const std::wstring& sourcePath, std::int64_t diskGroupId = 0) {
    wit::core::Disk disk;
    disk.diskGroupId = diskGroupId;
    disk.diskName = name;
    disk.diskNumber = 1;
    disk.sourcePath = sourcePath;
    disk.totalCapacity = 1000;
    disk.freeSpace = 100;
    disk.diskType = wit::core::DiskType::VirtualDisk;
    return disk;
}
}

TEST(OwnerDataPageCache, NormalizesRowsToPageStarts) {
    wit::ui::OwnerDataPageCache<int> cache(10, 2);

    EXPECT_EQ(cache.NormalizeStart(0), 0);
    EXPECT_EQ(cache.NormalizeStart(9), 0);
    EXPECT_EQ(cache.NormalizeStart(10), 10);
    EXPECT_EQ(cache.NormalizeStart(27), 20);
}

TEST(OwnerDataPageCache, ReturnsCachedEntriesAndRefreshesUsage) {
    wit::ui::OwnerDataPageCache<int> cache(10, 2);
    cache.StorePage({0, {1, 2, 3}});

    ASSERT_NE(cache.EntryAt(1, 100), nullptr);
    EXPECT_EQ(*cache.EntryAt(1, 100), 2);
    EXPECT_EQ(cache.EntryAt(8, 100), nullptr);
    EXPECT_EQ(cache.EntryAt(100, 100), nullptr);
}

TEST(OwnerDataPageCache, EvictsLeastRecentlyUsedPage) {
    wit::ui::OwnerDataPageCache<int> cache(10, 2);
    cache.StorePage({0, {1}});
    cache.StorePage({10, {2}});
    ASSERT_NE(cache.EntryAt(0, 100), nullptr);

    cache.StorePage({20, {3}});

    EXPECT_TRUE(cache.ContainsStart(0));
    EXPECT_FALSE(cache.ContainsStart(10));
    EXPECT_TRUE(cache.ContainsStart(20));
}

TEST(OwnerDataPageCache, PendingPageStartCanBeReplacedAndConsumed) {
    wit::ui::OwnerDataPageCache<int> cache(10, 2);

    cache.SetPendingStart(10);
    cache.SetPendingStart(20);

    EXPECT_TRUE(cache.HasPendingStart());
    EXPECT_EQ(cache.TakePendingStart(), 20);
    EXPECT_FALSE(cache.HasPendingStart());
}

TEST(OwnerDataPageCache, RequestIdsRejectStaleResults) {
    wit::ui::OwnerDataPageCache<int> cache(10, 2);

    const auto stale = cache.BeginRequest();
    const auto current = cache.BeginRequest();

    EXPECT_FALSE(cache.IsCurrentRequest(stale));
    EXPECT_TRUE(cache.IsCurrentRequest(current));
    cache.InvalidateRequests();
    EXPECT_FALSE(cache.IsCurrentRequest(current));
}
TEST(SearchPaneIcons, OwnsAnIndependentImageList) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    const auto owner = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED,
        0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(owner, nullptr);
    const auto browserList = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | LVS_REPORT | LVS_SHAREIMAGELISTS, 0, 0, 100, 100,
        owner, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FILES)),
        GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(browserList, nullptr);
    const auto browserImages = ImageList_Create(16, 16, ILC_COLOR32, 1, 0);
    ASSERT_NE(browserImages, nullptr);
    HBITMAP bitmap = CreateBitmap(16, 16, 1, 32, nullptr);
    ASSERT_NE(bitmap, nullptr);
    ASSERT_EQ(ImageList_Add(browserImages, bitmap, nullptr), 0);
    DeleteObject(bitmap);
    ListView_SetImageList(browserList, browserImages, LVSIL_SMALL);

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(owner, &repository, [] {}));
    PumpMessages();
    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto searchList = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    ASSERT_NE(searchList, nullptr);
    const auto searchImages = ListView_GetImageList(searchList, LVSIL_SMALL);
    ASSERT_NE(searchImages, nullptr);
    EXPECT_NE(searchImages, browserImages);
    EXPECT_EQ(ImageList_GetImageCount(searchImages), 105);

    dialog.Close();
    PumpMessages();
    EXPECT_EQ(ImageList_GetImageCount(browserImages), 1)
        << "closing search must not destroy the tree/main-list image list";

    ImageList_Destroy(browserImages);
    DestroyWindow(owner);
}
TEST(SearchPaneLifetime, RebindingCancelsTheOldRepositoryBeforeReplacement) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    BlockingSearchRepository first;
    ImmediateSearchRepository second;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &first, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    ASSERT_TRUE(SetDlgItemTextW(searchWindow, IDC_SEARCH_NAME, L"*"));
    SendMessageW(GetDlgItem(searchWindow, IDC_SEARCH_EXECUTE), BM_CLICK, 0, 0);
    while (!first.countStarted.load()) std::this_thread::yield();

    ASSERT_TRUE(dialog.Show(nullptr, &second, [] {}));
    EXPECT_GT(first.cancelCalls.load(), 0);

    const auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(results) != 1 && std::chrono::steady_clock::now() < deadline) {
        PumpMessages();
        Sleep(1);
    }
    EXPECT_EQ(ListView_GetItemCount(results), 1);

    dialog.Close();
    PumpMessages();
}

TEST(CatalogTreeViewLazyLoading, AddCatalogLoadsOnlyRootUntilExpanded) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_TREEVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-tree-lazy-" + std::to_wstring(GetCurrentProcessId()));
    const auto catalogPath = testRoot / L"tree.db";
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);

    wit::storage::Database database;
    ASSERT_TRUE(database.CreateNew(catalogPath.wstring(), true));
    const auto rootGroupId = database.CreateDiskGroup(L"RootGroup");
    const auto nestedGroupId = database.CreateDiskGroup(L"NestedGroup");
    ASSERT_NE(rootGroupId, 0);
    ASSERT_NE(nestedGroupId, 0);
    ASSERT_TRUE(database.MoveDiskGroupToGroup(nestedGroupId, rootGroupId));
    ASSERT_NE(database.AddDisk(TestDisk(L"RootDisk", L"R:\\")), 0);
    ASSERT_NE(database.AddDisk(TestDisk(L"GroupedDisk", L"G:\\", rootGroupId)), 0);

    const HWND tree = CreateWindowExW(0, WC_TREEVIEWW, L"", WS_POPUP | TVS_HASBUTTONS | TVS_HASLINES,
        0, 0, 320, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(tree, nullptr);

    wit::ui::CatalogTreeView catalogTree;
    catalogTree.Attach(tree, [&](wit::core::CatalogId) { return &database; });
    catalogTree.AddCatalog(1, L"Catalog", &database, true);

    const auto root = TreeView_GetRoot(tree);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(TreeChildCount(tree, root), 0);

    catalogTree.Expand(root);
    EXPECT_EQ(TreeChildCount(tree, root), 2);
    const auto rootGroup = FindDisplayedDiskGroup(catalogTree, tree, root, rootGroupId);
    ASSERT_NE(rootGroup, nullptr);
    EXPECT_EQ(TreeChildCount(tree, rootGroup), 0);

    catalogTree.Expand(rootGroup);
    EXPECT_EQ(TreeChildCount(tree, rootGroup), 2);
    EXPECT_NE(FindDisplayedDiskGroup(catalogTree, tree, rootGroup, nestedGroupId), nullptr);

    DestroyWindow(tree);
    database.Close();
    std::filesystem::remove_all(testRoot);
}

TEST(CatalogTreeViewLazyLoading, SelectLocationExpandsNestedDiskGroups) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_TREEVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-tree-select-" + std::to_wstring(GetCurrentProcessId()));
    const auto catalogPath = testRoot / L"tree.db";
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);

    wit::storage::Database database;
    ASSERT_TRUE(database.CreateNew(catalogPath.wstring(), true));
    const auto rootGroupId = database.CreateDiskGroup(L"RootGroup");
    const auto nestedGroupId = database.CreateDiskGroup(L"NestedGroup");
    ASSERT_NE(rootGroupId, 0);
    ASSERT_NE(nestedGroupId, 0);
    ASSERT_TRUE(database.MoveDiskGroupToGroup(nestedGroupId, rootGroupId));

    const HWND tree = CreateWindowExW(0, WC_TREEVIEWW, L"", WS_POPUP | TVS_HASBUTTONS | TVS_HASLINES,
        0, 0, 320, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(tree, nullptr);

    wit::ui::CatalogTreeView catalogTree;
    catalogTree.Attach(tree, [&](wit::core::CatalogId) { return &database; });
    catalogTree.AddCatalog(1, L"Catalog", &database, true);

    wit::core::BrowserTarget target;
    target.catalogId = 1;
    target.location.isRoot = false;
    target.location.isDiskGroup = true;
    target.location.diskGroupId = nestedGroupId;
    target.location.diskGroupName = L"NestedGroup";

    EXPECT_TRUE(catalogTree.SelectLocation(target));
    const auto selected = TreeView_GetSelection(tree);
    ASSERT_NE(selected, nullptr);
    const auto* selectedTarget = catalogTree.TargetFor(selected);
    ASSERT_NE(selectedTarget, nullptr);
    EXPECT_TRUE(selectedTarget->location.isDiskGroup);
    EXPECT_EQ(selectedTarget->location.diskGroupId, nestedGroupId);

    DestroyWindow(tree);
    database.Close();
    std::filesystem::remove_all(testRoot);
}
TEST(CatalogTreeViewLazyLoading, SelectLocationExpandsDiskGroupsToFindSource) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_TREEVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-tree-select-source-" + std::to_wstring(GetCurrentProcessId()));
    const auto catalogPath = testRoot / L"tree.db";
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);

    wit::storage::Database database;
    ASSERT_TRUE(database.CreateNew(catalogPath.wstring(), true));
    const auto rootGroupId = database.CreateDiskGroup(L"RootGroup");
    const auto nestedGroupId = database.CreateDiskGroup(L"NestedGroup");
    ASSERT_NE(rootGroupId, 0);
    ASSERT_NE(nestedGroupId, 0);
    ASSERT_TRUE(database.MoveDiskGroupToGroup(nestedGroupId, rootGroupId));

    wit::core::Disk disk{};
    disk.diskName = L"DiskInGroup";
    disk.diskNumber = 1;
    disk.sourcePath = L"X:\\DiskInGroup";
    disk.totalCapacity = 1024;
    disk.freeSpace = 512;
    disk.addedAt = 100;
    disk.updatedAt = 100;
    disk.diskType = wit::core::DiskType::VirtualDisk;
    disk.id = database.AddDisk(disk);
    ASSERT_NE(disk.id, 0);
    ASSERT_TRUE(database.MoveDiskToGroup(disk.id, nestedGroupId));

    const HWND tree = CreateWindowExW(0, WC_TREEVIEWW, L"", WS_POPUP | TVS_HASBUTTONS | TVS_HASLINES,
        0, 0, 320, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(tree, nullptr);

    wit::ui::CatalogTreeView catalogTree;
    catalogTree.Attach(tree, [&](wit::core::CatalogId) { return &database; });
    catalogTree.AddCatalog(1, L"Catalog", &database, true);

    wit::core::BrowserTarget target;
    target.catalogId = 1;
    target.location.isRoot = false;
    target.location.sourceId = disk.id;
    target.location.sourceName = disk.diskName;
    target.location.sourceRoot = disk.sourcePath;
    target.location.path = disk.sourcePath;

    EXPECT_TRUE(catalogTree.SelectLocation(target));
    const auto selected = TreeView_GetSelection(tree);
    ASSERT_NE(selected, nullptr);
    const auto* selectedTarget = catalogTree.TargetFor(selected);
    ASSERT_NE(selectedTarget, nullptr);
    EXPECT_FALSE(selectedTarget->location.isDiskGroup);
    EXPECT_EQ(selectedTarget->location.sourceId, disk.id);
    EXPECT_EQ(selectedTarget->location.path, disk.sourcePath);

    DestroyWindow(tree);
    database.Close();
    std::filesystem::remove_all(testRoot);
}
TEST(FileListViewPerformance, StaleLoadMessageDoesNotCancelCurrentFolderLoad) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    BlockingBrowserRepository repository;
    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = 1;
    location.path = L"X:\\Folder";
    fileListView.SetLocation(location, wit::storage::MakeBrowserReadContext(&repository));

    const auto startedDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!repository.countStarted.load() && std::chrono::steady_clock::now() < startedDeadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_TRUE(repository.countStarted.load());

    SendMessageW(fileList, WM_APP + 47, 0, 0);
    repository.releaseCount = true;

    const auto loadedDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(fileList) != 1 && std::chrono::steady_clock::now() < loadedDeadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(fileList), 1);
    ASSERT_NE(fileListView.CachedEntryAt(0), nullptr);
    DestroyWindow(fileList);
}



TEST(FileListViewPerformance, DatabaseCloseInvalidatesBrowserReadContext) {
    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-browser-read-context-" + std::to_wstring(GetCurrentProcessId()));
    const auto catalogPath = testRoot / L"context.db";
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);

    wit::storage::Database database;
    ASSERT_TRUE(database.CreateNew(catalogPath.wstring(), true));
    auto context = database.CreateBrowserReadContext();
    ASSERT_TRUE(context.IsActive());

    database.Close();
    EXPECT_FALSE(context.IsActive());

    std::filesystem::remove_all(testRoot);
}
TEST(FileListViewPerformance, InvalidatedBrowserReadContextSuppressesBlockedLoadResult) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    BlockingBrowserRepository repository;
    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = 1;
    location.path = L"X:\\Folder";
    auto context = wit::storage::MakeBrowserReadContext(&repository);
    fileListView.SetLocation(location, context);

    const auto startedDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!repository.countStarted.load() && std::chrono::steady_clock::now() < startedDeadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_TRUE(repository.countStarted.load());

    context.lifetimeToken->Invalidate();
    repository.releaseCount = true;

    const auto quietDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (std::chrono::steady_clock::now() < quietDeadline) {
        PumpMessages();
        Sleep(1);
    }
    EXPECT_EQ(ListView_GetItemCount(fileList), 0);
    EXPECT_EQ(fileListView.CachedEntryAt(0), nullptr);
    DestroyWindow(fileList);
}
TEST(FileListViewPerformance, CacheHintDoesNotSynchronouslyReadPages) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    CountingBrowserRepository repository;
    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = 1;
    location.path = L"X:\\FakeSearchStressDisk";
    fileListView.SetLocation(location, wit::storage::MakeBrowserReadContext(&repository));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(fileList) != 500000 && std::chrono::steady_clock::now() < deadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(fileList), 500000);
    EXPECT_EQ(repository.pageCalls, 1);

    fileListView.PreloadRange(250000, 250080);
    EXPECT_EQ(repository.pageCalls, 1);

    const auto pageDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!fileListView.CachedEntryAt(250000) && std::chrono::steady_clock::now() < pageDeadline) {
        PumpMessages();
        Sleep(1);
    }
    const auto* entry = fileListView.CachedEntryAt(250000);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(repository.pageCalls, 2);
    DestroyWindow(fileList);
}

TEST(FileListViewPerformance, EntryAtSchedulesMissingPageWithoutSynchronouslyReading) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    CountingBrowserRepository repository;
    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = 1;
    location.path = L"X:\\FakeSearchStressDisk";
    fileListView.SetLocation(location, wit::storage::MakeBrowserReadContext(&repository));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(fileList) != 500000 && std::chrono::steady_clock::now() < deadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(fileList), 500000);
    EXPECT_EQ(repository.pageCalls, 1);

    EXPECT_EQ(fileListView.EntryAt(250000), nullptr);

    DestroyWindow(fileList);
}

TEST(FileListViewPerformance, QueuedEntrySelectionRestoresAfterAsyncLoad) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    BlockingBrowserRepository repository;
    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = 1;
    location.path = L"X:\\FakeSearchStressDisk";
    fileListView.SetLocation(location, wit::storage::MakeBrowserReadContext(&repository));

    const auto countStartedDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!repository.countStarted.load() && std::chrono::steady_clock::now() < countStartedDeadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_TRUE(repository.countStarted.load());

    fileListView.QueueEntrySelection(1, false);
    repository.releaseCount = true;

    const auto loadDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(fileList) != 1 && std::chrono::steady_clock::now() < loadDeadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(fileList), 1);
    PumpMessages();

    EXPECT_EQ(ListView_GetNextItem(fileList, -1, LVNI_SELECTED), 0);
    EXPECT_EQ(ListView_GetNextItem(fileList, -1, LVNI_FOCUSED), 0);

    DestroyWindow(fileList);
}
TEST(FileListViewPerformance, SelectEntryDoesNotSynchronouslyScanLargeFolder) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    CountingBrowserRepository repository;
    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = 1;
    location.path = L"X:\\FakeSearchStressDisk";
    fileListView.SetLocation(location, wit::storage::MakeBrowserReadContext(&repository));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(fileList) != 500000 && std::chrono::steady_clock::now() < deadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(fileList), 500000);
    ASSERT_EQ(repository.pageCalls, 1);

    EXPECT_FALSE(fileListView.SelectEntry(400001, false));
    EXPECT_EQ(repository.pageCalls, 1) << "SelectEntry must not page through uncached rows synchronously";

    DestroyWindow(fileList);
}

TEST(SearchPaneColumns, LoadsAndPersistsIndependentWidths) {
    AppSettingsGuard settingsGuard;
    auto settings = wit::platform::LoadAppSettings();
    settings.fileListColumnWidths[L"BrowserContent.Name"] = 222;
    settings.searchListColumnWidths[L"SearchResults.Name"] = 321;
    ASSERT_TRUE(wit::platform::SaveAppSettings(settings));

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);
    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);
    fileListView.SetLocation({}, {});
    EXPECT_EQ(ListView_GetColumnWidth(fileList, 0), 222);
    DestroyWindow(fileList);

    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();

    auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(ListView_GetColumnWidth(results, 0), 321);

    constexpr std::array<int, 5> widths{333, 144, 155, 366, 177};
    constexpr std::array<const wchar_t*, 5> keys{
        L"SearchResults.Name", L"SearchResults.Type", L"SearchResults.Size",
        L"SearchResults.Path", L"SearchResults.Modified"
    };
    for (std::size_t index = 0; index < widths.size(); ++index) {
        ASSERT_TRUE(ListView_SetColumnWidth(results, static_cast<int>(index), widths[index]));
    }

    // Persist when the user releases the mouse after dragging a real search header divider.
    SendMessageW(ListView_GetHeader(results), WM_LBUTTONUP, 0, 0);
    PumpMessages();

    auto persisted = wit::platform::LoadAppSettings();
    for (std::size_t index = 0; index < keys.size(); ++index) {
        const auto saved = persisted.searchListColumnWidths.find(keys[index]);
        ASSERT_NE(saved, persisted.searchListColumnWidths.end());
        EXPECT_EQ(saved->second, widths[index]);
    }
    EXPECT_EQ(persisted.fileListColumnWidths.at(L"BrowserContent.Name"), 222);

    // Saving a partial or stale settings snapshot must never delete existing column widths.
    persisted.fileListColumnWidths.erase(L"BrowserContent.Name");
    persisted.searchListColumnWidths.erase(L"SearchResults.Path");
    ASSERT_TRUE(wit::platform::SaveAppSettings(persisted));
    persisted = wit::platform::LoadAppSettings();
    EXPECT_EQ(persisted.fileListColumnWidths.at(L"BrowserContent.Name"), 222);
    EXPECT_EQ(persisted.searchListColumnWidths.at(L"SearchResults.Path"), widths[3]);

    dialog.Close();
    PumpMessages();
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();
    searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    ASSERT_NE(results, nullptr);
    for (std::size_t index = 0; index < widths.size(); ++index) {
        EXPECT_EQ(ListView_GetColumnWidth(results, static_cast<int>(index)), widths[index]);
    }

    dialog.Close();
    PumpMessages();
}

TEST(SearchPaneHistory, PersistsLastTwentyAndNavigatesWithArrows) {
    AppSettingsGuard settingsGuard;
    auto settings = wit::platform::LoadAppSettings();
    settings.quickSearchHistory.clear();
    for (int index = 1; index <= 21; ++index) {
        wit::platform::RememberQuickSearchQuery(settings, L"query " + std::to_wstring(index));
    }
    ASSERT_TRUE(wit::platform::SaveAppSettings(settings));

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto edit = GetDlgItem(searchWindow, IDC_SEARCH_NAME);
    ASSERT_NE(edit, nullptr);

    ASSERT_TRUE(SetWindowTextW(edit, L"draft"));
    SendMessageW(edit, WM_KEYDOWN, VK_UP, 0);
    EXPECT_EQ(WindowText(edit), L"query 21");
    EXPECT_EQ(SelectionStart(edit), WindowText(edit).size());
    SendMessageW(edit, WM_KEYDOWN, VK_UP, 0);
    EXPECT_EQ(WindowText(edit), L"query 20");
    EXPECT_EQ(SelectionStart(edit), WindowText(edit).size());
    SendMessageW(edit, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(WindowText(edit), L"query 21");
    EXPECT_EQ(SelectionStart(edit), WindowText(edit).size());
    SendMessageW(edit, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(WindowText(edit), L"draft");
    EXPECT_EQ(SelectionStart(edit), WindowText(edit).size());
    SendMessageW(edit, EM_SETSEL, 2, 2);
    SendMessageW(edit, WM_KEYDOWN, VK_DOWN, 0);
    EXPECT_EQ(WindowText(edit), L"draft");
    EXPECT_EQ(SelectionStart(edit), 2u);

    ASSERT_TRUE(SetWindowTextW(edit, L"query 22"));
    SendMessageW(GetDlgItem(searchWindow, IDC_SEARCH_EXECUTE), BM_CLICK, 0, 0);
    PumpMessages();

    const auto saved = wit::platform::LoadAppSettings();
    ASSERT_EQ(saved.quickSearchHistory.size(), 20u);
    EXPECT_EQ(saved.quickSearchHistory.front(), L"query 22");
    EXPECT_EQ(std::ranges::find(saved.quickSearchHistory, L"query 1"), saved.quickSearchHistory.end());
    EXPECT_EQ(std::ranges::find(saved.quickSearchHistory, L"query 2"), saved.quickSearchHistory.end());

    dialog.Close();
    PumpMessages();
}

TEST(SearchPaneStatus, ShowsCountFocusedSelectionAndElapsedTime) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    const auto status = GetDlgItem(searchWindow, IDC_SEARCH_STATUS);
    ASSERT_NE(results, nullptr);
    ASSERT_NE(status, nullptr);

    ASSERT_TRUE(SetDlgItemTextW(searchWindow, IDC_SEARCH_NAME, L"*"));
    SendMessageW(GetDlgItem(searchWindow, IDC_SEARCH_EXECUTE), BM_CLICK, 0, 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(results) != 1 && std::chrono::steady_clock::now() < deadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(results), 1);

    ListView_SetItemState(results, 0, LVIS_SELECTED | LVIS_FOCUSED,
        LVIS_SELECTED | LVIS_FOCUSED);
    PumpMessages();

    EXPECT_EQ(StatusPartText(status, 0), L"Items on list: 1");
    EXPECT_NE(StatusPartText(status, 1).find(L"replacement.txt, 700 MB"), std::wstring::npos);
    EXPECT_EQ(StatusPartText(status, 2), L"Selected items: 1 (total 700 MB)");
    const auto elapsed = StatusPartText(status, 3);
    EXPECT_TRUE(elapsed.ends_with(L" s"));

    dialog.Close();
    PumpMessages();
}

TEST(SearchPaneAdvancedSearch, ClearResetsResultsAndStatus) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    ImmediateSearchRepository repository;
    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &repository, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    const auto status = GetDlgItem(searchWindow, IDC_SEARCH_STATUS);
    const auto summary = GetDlgItem(searchWindow, IDC_SEARCH_SUMMARY);
    const auto query = GetDlgItem(searchWindow, IDC_ADVANCED_SEARCH_QUERY);
    ASSERT_NE(results, nullptr);
    ASSERT_NE(status, nullptr);
    ASSERT_NE(summary, nullptr);
    ASSERT_NE(query, nullptr);

    ASSERT_TRUE(SetWindowTextW(query, L"filename = \"replacement.txt\""));
    SendMessageW(GetDlgItem(searchWindow, IDC_ADVANCED_SEARCH_EXECUTE), BM_CLICK, 0, 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (ListView_GetItemCount(results) != 1 && std::chrono::steady_clock::now() < deadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(results), 1);
    EXPECT_EQ(StatusPartText(status, 0), L"Items on list: 1");

    SendMessageW(GetDlgItem(searchWindow, IDC_ADVANCED_SEARCH_CLEAR), BM_CLICK, 0, 0);
    PumpMessages();

    EXPECT_EQ(WindowText(query), L"");
    EXPECT_EQ(ListView_GetItemCount(results), 0);
    EXPECT_EQ(WindowText(summary), L"Enter advanced search criteria.");
    EXPECT_EQ(StatusPartText(status, 0), L"Items on list: 0");

    dialog.Close();
    PumpMessages();
}

TEST(BrowserFileListPerformance, PagesFakeCatalogByNameWithoutBlocking) {
    const auto catalogPath = std::filesystem::current_path() / L"tools" / L"catalog-test" /
        L"fake-search-catalog.sqlite";
    if (!std::filesystem::exists(catalogPath)) {
        GTEST_SKIP() << "tools\\catalog-test\\fake-search-catalog.sqlite is not available";
    }

    wit::storage::Database database;
    ASSERT_TRUE(database.OpenExisting(catalogPath.wstring()));
    const auto disks = database.GetDisksPage(0, 1);
    ASSERT_FALSE(disks.empty());

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = disks.front().id;
    location.sourceName = disks.front().diskName;
    location.sourceRoot = disks.front().sourcePath;
    location.path = disks.front().sourcePath;

    if (database.GetBrowserItemCount(location) != 500000) {
        const auto rootItems = database.GetBrowserItemsPage(location, 0, 10, {});
        const auto stressFolder = std::ranges::find_if(rootItems, [](const auto& item) {
            return item.isDirectory;
        });
        ASSERT_NE(stressFolder, rootItems.end());
        location.path += L"\\" + stressFolder->name;
    }
    ASSERT_EQ(database.GetBrowserItemCount(location), 500000);

    const auto started = std::chrono::steady_clock::now();
    for (int offset : {0, 128000, 256000, 400000}) {
        const auto page = database.GetBrowserItemsPage(location, offset, 512, {});
        ASSERT_EQ(page.size(), 512u);
    }
    const auto elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    EXPECT_LT(elapsedMs, 150.0) << "default browser scrolling must stay on the indexed name path";
}


TEST(BrowserFileListPerformance, MainListScrollAndSortFakeCatalogDoesNotGoBlank) {
    const auto catalogPath = std::filesystem::current_path() / L"tools" / L"catalog-test" /
        L"fake-search-catalog.sqlite";
    if (!std::filesystem::exists(catalogPath)) {
        GTEST_SKIP() << "tools\\catalog-test\\fake-search-catalog.sqlite is not available";
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    wit::storage::Database database;
    ASSERT_TRUE(database.OpenExisting(catalogPath.wstring()));
    const auto disks = database.GetDisksPage(0, 1);
    ASSERT_FALSE(disks.empty());

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = disks.front().id;
    location.sourceName = disks.front().diskName;
    location.sourceRoot = disks.front().sourcePath;
    location.path = disks.front().sourcePath;

    if (database.GetBrowserItemCount(location) != 500000) {
        const auto rootItems = database.GetBrowserItemsPage(location, 0, 10, {});
        const auto stressFolder = std::ranges::find_if(rootItems, [](const auto& item) {
            return item.isDirectory;
        });
        ASSERT_NE(stressFolder, rootItems.end());
        location.path += L"\\" + stressFolder->name;
    }
    ASSERT_EQ(database.GetBrowserItemCount(location), 500000);

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 900, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    const auto openStarted = std::chrono::steady_clock::now();
    const double openDispatchMs = MeasureMilliseconds([&] {
        fileListView.SetLocation(location, database.CreateBrowserReadContext());
    });
    EXPECT_LT(openDispatchMs, 250.0) << "main list open must not count/page on the UI thread";

    auto waitForRow = [&](int row) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!fileListView.CachedEntryAt(row) && std::chrono::steady_clock::now() < deadline) {
            fileListView.PreloadRange(row, row);
            PumpMessages();
            Sleep(1);
        }
        return fileListView.CachedEntryAt(row) != nullptr;
    };

    const auto loadDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (ListView_GetItemCount(fileList) != 500000 && std::chrono::steady_clock::now() < loadDeadline) {
        PumpMessages();
        Sleep(1);
    }
    ASSERT_EQ(ListView_GetItemCount(fileList), 500000);
    ASSERT_TRUE(waitForRow(0));
    const double loadCompletionMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - openStarted).count();

    const auto scrollStarted = std::chrono::steady_clock::now();
    ASSERT_TRUE(ListView_EnsureVisible(fileList, 250000, FALSE));
    fileListView.PreloadRange(250000, 250000);
    ASSERT_TRUE(ListView_EnsureVisible(fileList, 400000, FALSE));
    fileListView.PreloadRange(400000, 400000);
    ASSERT_TRUE(waitForRow(400000)) << "latest scrolled viewport page should load after an older page finishes";
    const double scrollFillMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - scrollStarted).count();

    wchar_t text[260]{};
    fileListView.TextFor(400000, 0, text, std::size(text));
    EXPECT_NE(std::wstring(text), L"");

    const auto sortStarted = std::chrono::steady_clock::now();
    const double sortDispatchMs = MeasureMilliseconds([&] {
        fileListView.SetSort({wit::core::FileSortColumn::Type, true});
    });
    EXPECT_LT(sortDispatchMs, 250.0) << "main list type sort must not block the UI thread";
    ASSERT_TRUE(waitForRow(ListView_GetTopIndex(fileList)));
    const double sortFillMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - sortStarted).count();
    text[0] = L'\0';
    fileListView.TextFor(ListView_GetTopIndex(fileList), 0, text, std::size(text));
    EXPECT_NE(std::wstring(text), L"");

    const auto typeScrollStarted = std::chrono::steady_clock::now();
    ASSERT_TRUE(ListView_EnsureVisible(fileList, 128000, FALSE));
    fileListView.PreloadRange(128000, 128000);
    ASSERT_TRUE(waitForRow(128000)) << "type-sorted deep scroll page should not stay blank";
    const double typeScrollFillMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - typeScrollStarted).count();
    text[0] = L'\0';
    fileListView.TextFor(128000, 0, text, std::size(text));
    EXPECT_NE(std::wstring(text), L"");

    std::cout << "MAIN_LIST_PERF fake_catalog_items=500000"
        << " open_dispatch_ms=" << openDispatchMs
        << " load_completion_ms=" << loadCompletionMs
        << " scroll_fill_ms=" << scrollFillMs
        << " type_sort_dispatch_ms=" << sortDispatchMs
        << " type_sort_fill_ms=" << sortFillMs
        << " type_scroll_fill_ms=" << typeScrollFillMs
        << std::endl;

    DestroyWindow(fileList);
    database.Close();
}

TEST(BrowserFileListPerformance, MainListUsesStoredFolderPathWhenNameIsNotALeaf) {
    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-main-list-stored-path-" + std::to_wstring(GetCurrentProcessId()));
    const auto catalogPath = testRoot / L"stored-path.db";
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);

    wit::storage::Database database;
    ASSERT_TRUE(database.CreateNew(catalogPath.wstring(), true));

    wit::core::Disk disk{};
    disk.diskName = L"Imported";
    disk.diskNumber = 1;
    disk.sourcePath = L"ImportedDisk";
    disk.totalCapacity = 1024;
    disk.freeSpace = 512;
    disk.addedAt = 100;
    disk.updatedAt = 100;
    disk.diskType = wit::core::DiskType::VirtualDisk;
    disk.id = database.AddDisk(disk);
    ASSERT_NE(disk.id, 0);

    wit::core::FolderEntry root{};
    root.diskId = disk.id;
    root.path = disk.sourcePath;
    root.name = L"ImportedDisk";
    root.modifiedAt = 100;
    root.attributes = FILE_ATTRIBUTE_DIRECTORY;
    root.id = database.InsertFolder(root);
    ASSERT_NE(root.id, 0);

    wit::core::FolderEntry parent{};
    parent.diskId = disk.id;
    parent.parentFolderId = root.id;
    parent.hasParent = true;
    parent.path = wit::platform::Join(root.path, L"parent");
    parent.name = L"parent";
    parent.modifiedAt = 101;
    parent.attributes = FILE_ATTRIBUTE_DIRECTORY;
    parent.id = database.InsertFolder(parent);
    ASSERT_NE(parent.id, 0);

    wit::core::FolderEntry imported{};
    imported.diskId = disk.id;
    imported.parentFolderId = parent.id;
    imported.hasParent = true;
    imported.path = wit::platform::Join(parent.path, L"actual-child");
    imported.name = imported.path;
    imported.modifiedAt = 102;
    imported.attributes = FILE_ATTRIBUTE_DIRECTORY;
    imported.id = database.InsertFolder(imported);
    ASSERT_NE(imported.id, 0);

    wit::core::FileEntry nestedFile{};
    nestedFile.catalogId = disk.id;
    nestedFile.folderId = imported.id;
    nestedFile.name = L"inside.txt";
    nestedFile.extension = L"txt";
    nestedFile.size = 5;
    nestedFile.modifiedAt = 103;
    nestedFile.attributes = FILE_ATTRIBUTE_ARCHIVE;
    ASSERT_TRUE(database.InsertFile(nestedFile));

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 900, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    auto waitForCount = [&](int expected) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (ListView_GetItemCount(fileList) != expected && std::chrono::steady_clock::now() < deadline) {
            PumpMessages();
            Sleep(1);
        }
        return ListView_GetItemCount(fileList) == expected;
    };
    auto waitForRow = [&](int row) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!fileListView.CachedEntryAt(row) && std::chrono::steady_clock::now() < deadline) {
            fileListView.PreloadRange(row, row);
            PumpMessages();
            Sleep(1);
        }
        return fileListView.CachedEntryAt(row) != nullptr;
    };

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = disk.id;
    location.sourceName = disk.diskName;
    location.sourceRoot = disk.sourcePath;
    location.path = parent.path;
    fileListView.SetLocation(location, database.CreateBrowserReadContext());
    ASSERT_TRUE(waitForCount(1));
    ASSERT_TRUE(waitForRow(0));
    const auto* folder = fileListView.CachedEntryAt(0);
    ASSERT_NE(folder, nullptr);
    ASSERT_TRUE(folder->isDirectory);
    ASSERT_EQ(folder->fullPath, imported.path);
    ASSERT_NE(wit::platform::Join(location.path, folder->name), imported.path);

    fileListView.SetLocation({false, false, 0, L"", disk.id, disk.diskName, disk.sourcePath, folder->fullPath},
        database.CreateBrowserReadContext());
    ASSERT_TRUE(waitForCount(1)) << "navigation must use folders.path, not parentPath + name";
    ASSERT_TRUE(waitForRow(0));

    wchar_t text[260]{};
    fileListView.TextFor(0, 0, text, std::size(text));
    EXPECT_EQ(std::wstring(text), L"inside.txt");

    DestroyWindow(fileList);
    database.Close();
    std::filesystem::remove_all(testRoot);
}
TEST(BrowserFileListPerformance, MainListFolderNavigationLoadsChildRows) {
    const auto testRoot = std::filesystem::temp_directory_path() /
        (L"whereisthat-main-list-navigation-" + std::to_wstring(GetCurrentProcessId()));
    const auto catalogPath = testRoot / L"nested.db";
    std::filesystem::remove_all(testRoot);
    std::filesystem::create_directories(testRoot);

    wit::storage::Database database;
    ASSERT_TRUE(database.CreateNew(catalogPath.wstring(), true));

    wit::core::Disk disk{};
    disk.diskName = L"Nested";
    disk.diskNumber = 1;
    disk.sourcePath = L"X:\\NestedDisk";
    disk.totalCapacity = 1024;
    disk.freeSpace = 512;
    disk.addedAt = 100;
    disk.updatedAt = 100;
    disk.diskType = wit::core::DiskType::VirtualDisk;
    disk.id = database.AddDisk(disk);
    ASSERT_NE(disk.id, 0);

    wit::core::FolderEntry root{};
    root.diskId = disk.id;
    root.path = disk.sourcePath;
    root.name = L"NestedDisk";
    root.modifiedAt = 100;
    root.attributes = FILE_ATTRIBUTE_DIRECTORY;
    root.contentSize = 5;
    root.id = database.InsertFolder(root);
    ASSERT_NE(root.id, 0);

    wit::core::FolderEntry child{};
    child.diskId = disk.id;
    child.parentFolderId = root.id;
    child.hasParent = true;
    child.path = wit::platform::Join(root.path, L"child");
    child.name = L"child";
    child.modifiedAt = 101;
    child.attributes = FILE_ATTRIBUTE_DIRECTORY;
    child.contentSize = 5;
    child.id = database.InsertFolder(child);
    ASSERT_NE(child.id, 0);

    wit::core::FileEntry nestedFile{};
    nestedFile.catalogId = disk.id;
    nestedFile.folderId = child.id;
    nestedFile.name = L"inside.txt";
    nestedFile.extension = L"txt";
    nestedFile.size = 5;
    nestedFile.modifiedAt = 102;
    nestedFile.attributes = FILE_ATTRIBUTE_ARCHIVE;
    ASSERT_TRUE(database.InsertFile(nestedFile));

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));

    const HWND fileList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_POPUP | LVS_REPORT | LVS_OWNERDATA,
        0, 0, 900, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ASSERT_NE(fileList, nullptr);

    wit::ui::FileListView fileListView;
    fileListView.Attach(fileList);

    auto waitForCount = [&](int expected) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (ListView_GetItemCount(fileList) != expected && std::chrono::steady_clock::now() < deadline) {
            PumpMessages();
            Sleep(1);
        }
        return ListView_GetItemCount(fileList) == expected;
    };
    auto waitForRow = [&](int row) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!fileListView.CachedEntryAt(row) && std::chrono::steady_clock::now() < deadline) {
            fileListView.PreloadRange(row, row);
            PumpMessages();
            Sleep(1);
        }
        return fileListView.CachedEntryAt(row) != nullptr;
    };

    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = disk.id;
    location.sourceName = disk.diskName;
    location.sourceRoot = disk.sourcePath;
    location.path = disk.sourcePath;
    fileListView.SetLocation(location, database.CreateBrowserReadContext());
    ASSERT_TRUE(waitForCount(1));
    ASSERT_TRUE(waitForRow(0));
    const auto* folder = fileListView.CachedEntryAt(0);
    ASSERT_NE(folder, nullptr);
    ASSERT_TRUE(folder->isDirectory);
    ASSERT_EQ(folder->name, L"child");

    location.path = wit::platform::Join(location.path, folder->name);
    fileListView.SetLocation(location, database.CreateBrowserReadContext());
    ASSERT_TRUE(waitForCount(1)) << "opening a folder from the main list must repopulate child rows";
    ASSERT_TRUE(waitForRow(0));

    wchar_t text[260]{};
    fileListView.TextFor(0, 0, text, std::size(text));
    EXPECT_EQ(std::wstring(text), L"inside.txt");

    DestroyWindow(fileList);
    database.Close();
    std::filesystem::remove_all(testRoot);
}
TEST(DISABLED_SearchPaneUiPerformance, SearchAndScrollFakeCatalog) {
    const auto catalogPath = std::filesystem::current_path() / L"tools" / L"catalog-test" /
        L"fake-search-catalog.sqlite";
    if (!std::filesystem::exists(catalogPath)) {
        // See tools/catalog-test/README.md for fixture generation instructions.
        GTEST_SKIP() << "tools\\catalog-test\\fake-search-catalog.sqlite is not available";
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    ASSERT_TRUE(InitCommonControlsEx(&controls));
    AtlModuleGuard module;
    ASSERT_TRUE(module.initialized());

    wit::storage::Database database;
    ASSERT_TRUE(database.OpenExisting(catalogPath.wstring()));

    wit::ui::SearchDialog dialog;
    ASSERT_TRUE(dialog.Show(nullptr, &database.SearchRepository(),
        [](const wit::core::FileEntry&) { return true; }, [] {}));
    PumpMessages();

    const auto searchWindow = FindWindowW(nullptr, L"Search for Items");
    ASSERT_NE(searchWindow, nullptr);
    const auto edit = GetDlgItem(searchWindow, IDC_SEARCH_NAME);
    const auto execute = GetDlgItem(searchWindow, IDC_SEARCH_EXECUTE);
    const auto results = GetDlgItem(searchWindow, IDC_SEARCH_RESULTS);
    ASSERT_NE(edit, nullptr);
    ASSERT_NE(execute, nullptr);
    ASSERT_NE(results, nullptr);

    ASSERT_TRUE(SetWindowTextW(edit, L"file"));
    const double searchDispatchMs = MeasureMilliseconds([&] {
        SendMessageW(execute, BM_CLICK, 0, 0);
    });
    EXPECT_LT(searchDispatchMs, 250.0) << "Search preparation must not block the UI thread";

    const auto completionStarted = std::chrono::steady_clock::now();
    int itemCount{};
    while (itemCount == 0 &&
        std::chrono::steady_clock::now() - completionStarted < std::chrono::seconds(30)) {
        PumpMessages();
        Sleep(10);
        itemCount = static_cast<int>(SendMessageW(results, LVM_GETITEMCOUNT, 0, 0));
    }
    const double searchCompletionMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - completionStarted).count();
    ASSERT_EQ(itemCount, 500000);

    const int pageIterations = 200;
    const double pageMs = MeasureMilliseconds([&] {
        for (int index = 0; index < pageIterations; ++index) {
            SendMessageW(results, WM_VSCROLL, SB_PAGEDOWN, 0);
            UpdateWindow(results);
        }
    });

    SendMessageW(results, LVM_ENSUREVISIBLE, 0, FALSE);
    UpdateWindow(results);

    const int lineIterations = 1000;
    const double lineMs = MeasureMilliseconds([&] {
        for (int index = 0; index < lineIterations; ++index) {
            SendMessageW(results, WM_VSCROLL, SB_LINEDOWN, 0);
            UpdateWindow(results);
        }
    });

    const int jumpIterations = 400;
    const double jumpMs = MeasureMilliseconds([&] {
        for (int index = 0; index < jumpIterations; ++index) {
            const auto item = (std::min)(itemCount - 1, index * 1024);
            SendMessageW(results, LVM_ENSUREVISIBLE, item, FALSE);
            UpdateWindow(results);
        }
    });

    std::cout << "SCROLL_PERF search_term=file"
        << " result_count=" << itemCount
        << " search_dispatch_ms=" << searchDispatchMs
        << " search_completion_ms=" << searchCompletionMs
        << " page_down_iterations=" << pageIterations
        << " page_down_total_ms=" << pageMs
        << " page_down_avg_ms=" << pageMs / pageIterations
        << " line_down_iterations=" << lineIterations
        << " line_down_total_ms=" << lineMs
        << " line_down_avg_ms=" << lineMs / lineIterations
        << " jump_iterations=" << jumpIterations
        << " jump_total_ms=" << jumpMs
        << " jump_avg_ms=" << jumpMs / jumpIterations
        << std::endl;

    dialog.Close();
    PumpMessages();
    database.Close();
}
