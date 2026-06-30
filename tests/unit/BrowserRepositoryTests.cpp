#include <gtest/gtest.h>
#include <wit_database/SqliteBrowserRepository.h>
#include <sqlite3.h>

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
}

TEST(BrowserRepository, SortsContentByNameWithSharedNaturalOrder) {
    EXPECT_EQ(BrowserNames({wit::core::FileSortColumn::Name, true}),
        (std::vector<std::wstring>{L"alpha", L"archive.zip", L"beta.txt", L"gamma.bin", L"item10.txt", L"item2.txt"}));
}

TEST(BrowserRepository, SortsContentByTypeAcrossFoldersAndFiles) {
    EXPECT_EQ(BrowserNames({wit::core::FileSortColumn::Type, true}),
        (std::vector<std::wstring>{L"archive.zip", L"alpha", L"gamma.bin", L"beta.txt", L"item10.txt", L"item2.txt"}));
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
        (std::vector<std::wstring>{L"alpha", L"archive.zip", L"beta.txt", L"gamma.bin", L"item10.txt", L"item2.txt"}));
}
