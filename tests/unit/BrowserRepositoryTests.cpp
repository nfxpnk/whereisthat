#include <gtest/gtest.h>
#include <wit_database/SqliteBrowserRepository.h>
#include <sqlite3.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {
class BrowserMemoryDatabase {
public:
    BrowserMemoryDatabase() {
        sqlite3_open(":memory:", &db_);
        Exec("CREATE TABLE folders(id INTEGER PRIMARY KEY,disk_id INTEGER,parent_folder_id INTEGER,path TEXT,"
            "name TEXT,content_size INTEGER,modified_at INTEGER,attributes INTEGER,entry_type TEXT);"
            "CREATE TABLE files(id INTEGER PRIMARY KEY,disk_id INTEGER,folder_id INTEGER,name TEXT,extension TEXT,"
            "size INTEGER,modified_at INTEGER,attributes INTEGER);"
            "CREATE TABLE disk_groups(id INTEGER PRIMARY KEY,parent_group_id INTEGER,name TEXT,updated_at INTEGER);"
            "CREATE TABLE disks(id INTEGER PRIMARY KEY,disk_group_id INTEGER,disk_name TEXT,disk_number INTEGER,"
            "source_path TEXT,total_capacity INTEGER,free_space INTEGER,updated_at INTEGER,description TEXT,"
            "category TEXT,location TEXT,disk_type TEXT);"
            "INSERT INTO folders(id,disk_id,parent_folder_id,path,name,content_size,modified_at,attributes,entry_type) "
            "VALUES(1,1,NULL,'C:\\','root',0,1,0,'directory'),"
            "(2,1,1,'C:\\alpha','alpha',90,20,0,'directory'),"
            "(3,1,1,'C:\\archive.zip','archive.zip',10,50,0,'archive');"
            "INSERT INTO files(id,disk_id,folder_id,name,extension,size,modified_at,attributes) "
            "VALUES(1,1,1,'beta.txt','txt',30,40,0),"
            "(2,1,1,'gamma.bin','bin',20,30,0),"
            "(3,1,1,'item2.txt','txt',70,60,0),"
            "(4,1,1,'item10.txt','txt',80,70,0);");
    }

    ~BrowserMemoryDatabase() {
        if (db_) sqlite3_close(db_);
    }

    sqlite3* Raw() const { return db_; }
    void Execute(const char* sql) { Exec(sql); }

private:
    void Exec(const char* sql) {
        ASSERT_EQ(sqlite3_exec(db_, sql, nullptr, nullptr, nullptr), SQLITE_OK);
    }

    sqlite3* db_{};
};

wit::core::BrowserLocation RootLocation() {
    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.sourceId = 1;
    location.path = L"C:\\";
    return location;
}

std::vector<std::wstring> Names(const std::vector<wit::core::FileEntry>& entries) {
    std::vector<std::wstring> names;
    for (const auto& entry : entries) names.push_back(entry.name);
    return names;
}

std::vector<std::wstring> BrowserNames(wit::core::FileSort sort) {
    BrowserMemoryDatabase database;
    wit::storage::SqliteBrowserRepository repository(database.Raw());
    return Names(repository.GetBrowserItemsPage(RootLocation(), 0, 20, sort));
}
std::vector<std::wstring> BrowserItemNames(const std::vector<wit::core::BrowserItem>& items) {
    std::vector<std::wstring> names;
    for (const auto& item : items) {
        names.push_back(item.type == wit::core::BrowserItemType::DiskGroup ? item.group.name : item.disk.diskName);
    }
    return names;
}

wit::core::BrowserLocation LocationForPath(const wchar_t* path) {
    auto location = RootLocation();
    location.path = path;
    return location;
}

wit::core::BrowserLocation GroupLocation(std::int64_t id, const wchar_t* name) {
    wit::core::BrowserLocation location;
    location.isRoot = false;
    location.isDiskGroup = true;
    location.diskGroupId = id;
    location.diskGroupName = name;
    return location;
}
}

TEST(BrowserRepository, SortsContentByNameWithSharedNaturalOrder) {
    EXPECT_EQ(BrowserNames({wit::core::FileSortColumn::Name, true}),
        (std::vector<std::wstring>{L"alpha", L"archive.zip", L"beta.txt", L"gamma.bin", L"item2.txt", L"item10.txt"}));
}

TEST(BrowserRepository, SortsContentByTypeAcrossFoldersAndFiles) {
    EXPECT_EQ(BrowserNames({wit::core::FileSortColumn::Type, true}),
        (std::vector<std::wstring>{L"archive.zip", L"alpha", L"gamma.bin", L"beta.txt", L"item2.txt", L"item10.txt"}));
}

TEST(BrowserRepository, SortsContentBySizeAcrossFoldersAndFiles) {
    EXPECT_EQ(BrowserNames({wit::core::FileSortColumn::Size, true}),
        (std::vector<std::wstring>{L"archive.zip", L"alpha", L"gamma.bin", L"beta.txt", L"item2.txt", L"item10.txt"}));
}

TEST(BrowserRepository, SortsContentByModifiedAcrossFoldersAndFiles) {
    EXPECT_EQ(BrowserNames({wit::core::FileSortColumn::Modified, true}),
        (std::vector<std::wstring>{L"alpha", L"archive.zip", L"gamma.bin", L"beta.txt", L"item2.txt", L"item10.txt"}));
}

TEST(BrowserRepository, SortingContentByPathDoesNotReturnBlankPage) {
    EXPECT_EQ(BrowserNames({wit::core::FileSortColumn::Path, true}),
        (std::vector<std::wstring>{L"alpha", L"archive.zip", L"beta.txt", L"gamma.bin", L"item2.txt", L"item10.txt"}));
}

TEST(BrowserRepository, EmptyFoldersAndMissingPathsReturnEmptyCountsAndPages) {
    BrowserMemoryDatabase database;
    database.Execute(
        "INSERT INTO folders(id,disk_id,parent_folder_id,path,name,content_size,modified_at,attributes,entry_type) "
        "VALUES(10,1,1,'C:\\empty','empty',0,80,0,'directory');");
    wit::storage::SqliteBrowserRepository repository(database.Raw());

    const auto empty = LocationForPath(L"C:\\empty");
    EXPECT_EQ(repository.GetBrowserItemCount(empty), 0);
    EXPECT_TRUE(repository.GetBrowserItemsPage(empty, 0, 20, {}).empty());
    EXPECT_FALSE(repository.HasChildFolders(1, empty.path));
    EXPECT_TRUE(repository.LastErrorMessage().empty());

    const auto missing = LocationForPath(L"C:\\missing");
    EXPECT_EQ(repository.GetBrowserItemCount(missing), 0);
    EXPECT_TRUE(repository.GetBrowserItemsPage(missing, 0, 20, {}).empty());
    EXPECT_FALSE(repository.HasChildFolders(1, missing.path));
    EXPECT_TRUE(repository.LastErrorMessage().empty());
}


TEST(BrowserRepository, ReportsSqlFailuresAndClearsErrorAfterSuccessfulRead) {
    BrowserMemoryDatabase database;
    wit::storage::SqliteBrowserRepository repository(database.Raw());

    database.Execute("DROP TABLE files;");

    EXPECT_EQ(repository.GetBrowserItemCount(RootLocation()), 0);
    const auto countError = repository.LastErrorMessage();
    EXPECT_NE(countError.find(L"Could not count browser items."), std::wstring::npos);
    EXPECT_NE(countError.find(L"no such table: files"), std::wstring::npos);

    EXPECT_TRUE(repository.GetBrowserItemsPage(RootLocation(), 0, 20, {}).empty());
    const auto pageError = repository.LastErrorMessage();
    EXPECT_NE(pageError.find(L"Could not read browser files."), std::wstring::npos);
    EXPECT_NE(pageError.find(L"no such table: files"), std::wstring::npos);

    EXPECT_EQ(repository.GetBrowserRootItemCount({}), 0);
    EXPECT_TRUE(repository.LastErrorMessage().empty());
}

TEST(BrowserRepository, SortsDuplicateNamesDeterministicallyWithinFoldersAndFiles) {
    BrowserMemoryDatabase database;
    database.Execute(
        "INSERT INTO folders(id,disk_id,parent_folder_id,path,name,content_size,modified_at,attributes,entry_type) "
        "VALUES(10,1,1,'C:\\duplicate-a','duplicate',11,80,0,'directory'),"
        "(11,1,1,'C:\\duplicate-b','duplicate',12,81,0,'directory');"
        "INSERT INTO files(id,disk_id,folder_id,name,extension,size,modified_at,attributes) "
        "VALUES(10,1,1,'duplicate','txt',13,82,0),(11,1,1,'duplicate','txt',14,83,0);");
    wit::storage::SqliteBrowserRepository repository(database.Raw());

    const auto entries = repository.GetBrowserItemsPage(RootLocation(), 0, 20,
        {wit::core::FileSortColumn::Name, true});
    std::vector<std::pair<std::int64_t, bool>> duplicates;
    for (const auto& entry : entries) {
        if (entry.name == L"duplicate") duplicates.push_back({entry.id, entry.isDirectory});
    }

    ASSERT_EQ(duplicates.size(), 4u);
    EXPECT_EQ(duplicates[0], (std::pair<std::int64_t, bool>{10, true}));
    EXPECT_EQ(duplicates[1], (std::pair<std::int64_t, bool>{11, true}));
    EXPECT_EQ(duplicates[2], (std::pair<std::int64_t, bool>{10, false}));
    EXPECT_EQ(duplicates[3], (std::pair<std::int64_t, bool>{11, false}));
}

TEST(BrowserRepository, BrowserContentKeepsFoldersBeforeFilesEvenWhenFileNameWouldSortFirst) {
    BrowserMemoryDatabase database;
    database.Execute(
        "INSERT INTO folders(id,disk_id,parent_folder_id,path,name,content_size,modified_at,attributes,entry_type) "
        "VALUES(10,1,1,'C:\\zzz-folder','zzz-folder',11,80,0,'directory');"
        "INSERT INTO files(id,disk_id,folder_id,name,extension,size,modified_at,attributes) "
        "VALUES(10,1,1,'aaa-file.txt','txt',13,82,0);");
    wit::storage::SqliteBrowserRepository repository(database.Raw());

    const auto names = Names(repository.GetBrowserItemsPage(RootLocation(), 0, 20,
        {wit::core::FileSortColumn::Name, true}));
    const auto folder = std::find(names.begin(), names.end(), L"zzz-folder");
    const auto file = std::find(names.begin(), names.end(), L"aaa-file.txt");

    ASSERT_NE(folder, names.end());
    ASSERT_NE(file, names.end());
    EXPECT_LT(folder, file) << "browser content intentionally groups folders before files";
}

TEST(BrowserRepository, RootAndDiskGroupPagingSortsGroupsBeforeDisksWithNaturalNames) {
    BrowserMemoryDatabase database;
    database.Execute(
        "INSERT INTO disk_groups(id,parent_group_id,name,updated_at) "
        "VALUES(1,NULL,'Group10',100),(2,NULL,'Group2',101),(3,1,'Nested2',102),(4,1,'Nested10',103);"
        "INSERT INTO disks(id,disk_group_id,disk_name,disk_number,source_path,total_capacity,free_space,updated_at,description,category,location,disk_type) "
        "VALUES(10,NULL,'Disk10',10,'D10',1000,100,110,'','','','VirtualDisk'),"
        "(11,NULL,'Disk2',2,'D2',900,90,111,'','','','VirtualDisk'),"
        "(12,1,'GroupedDisk',12,'G1',800,80,112,'','','','VirtualDisk');");
    wit::storage::SqliteBrowserRepository repository(database.Raw());

    EXPECT_EQ(repository.GetBrowserRootItemCount({}), 4);
    EXPECT_EQ(BrowserItemNames(repository.GetBrowserRootItemsPage({}, 0, 10, {0, true})),
        (std::vector<std::wstring>{L"Group2", L"Group10", L"Disk2", L"Disk10"}));
    EXPECT_EQ(BrowserItemNames(repository.GetBrowserRootItemsPage({}, 1, 2, {0, true})),
        (std::vector<std::wstring>{L"Group10", L"Disk2"}));

    const auto group = GroupLocation(1, L"Group10");
    EXPECT_EQ(repository.GetBrowserRootItemCount(group), 3);
    EXPECT_EQ(BrowserItemNames(repository.GetBrowserRootItemsPage(group, 0, 10, {0, true})),
        (std::vector<std::wstring>{L"Nested2", L"Nested10", L"GroupedDisk"}));
}

TEST(BrowserRepository, DuplicateFolderPathsArePreventedByCatalogSchemaAssumption) {
    BrowserMemoryDatabase database;
    ASSERT_EQ(sqlite3_exec(database.Raw(),
        "CREATE UNIQUE INDEX idx_folders_disk_path ON folders(disk_id, path COLLATE NOCASE);",
        nullptr, nullptr, nullptr), SQLITE_OK);

    EXPECT_EQ(sqlite3_exec(database.Raw(),
        "INSERT INTO folders(id,disk_id,parent_folder_id,path,name,content_size,modified_at,attributes,entry_type) "
        "VALUES(10,1,1,'c:\\ALPHA','alpha-copy',0,80,0,'directory');",
        nullptr, nullptr, nullptr), SQLITE_CONSTRAINT);
}
