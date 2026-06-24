#include <gtest/gtest.h>
#include <wit_search/SqliteSearchExecutor.h>
#include <sqlite3.h>
#include <string>

namespace {
class MemoryDatabase {
public:
    MemoryDatabase() {
        sqlite3_open(":memory:", &db_);
        Exec("CREATE TABLE folders(id INTEGER PRIMARY KEY,disk_id INTEGER,parent_folder_id INTEGER,path TEXT,"
            "name TEXT,content_size INTEGER,modified_at INTEGER,attributes INTEGER,entry_type TEXT);"
            "CREATE TABLE files(id INTEGER PRIMARY KEY,disk_id INTEGER,folder_id INTEGER,name TEXT,extension TEXT,"
            "size INTEGER,modified_at INTEGER,attributes INTEGER);"
            "INSERT INTO folders(id,disk_id,parent_folder_id,path,name,content_size,modified_at,attributes,entry_type) "
            "VALUES(1,1,NULL,'C:\\\\','alpha-folder',0,100,0,'directory'),"
            "(2,1,1,'C:\\\\alpha-folder','child',0,101,0,'directory');"
            "INSERT INTO files(id,disk_id,folder_id,name,extension,size,modified_at,attributes) "
            "VALUES(1,1,1,'alpha-file.txt','txt',42,102,0),(2,1,1,'beta-file.txt','txt',43,103,0),"
            "(3,1,1,'notes.txt.bak','bak',44,104,0),(4,1,1,'item10.txt','txt',45,105,0),"
            "(5,1,1,'item2.txt','txt',46,106,0);");
    }

    ~MemoryDatabase() {
        if (db_) sqlite3_close(db_);
    }

    MemoryDatabase(const MemoryDatabase&) = delete;
    MemoryDatabase& operator=(const MemoryDatabase&) = delete;

    sqlite3* Raw() const { return db_; }

private:
    void Exec(const char* sql) {
        ASSERT_EQ(sqlite3_exec(db_, sql, nullptr, nullptr, nullptr), SQLITE_OK);
    }

    sqlite3* db_{};
};

int TraceSearchCacheBuild(unsigned int, void* context, void* statement, void*) {
    const auto sql = std::string(sqlite3_sql(static_cast<sqlite3_stmt*>(statement)));
    if (sql.find("INSERT INTO wit_search_page_cache(is_directory,item_id) SELECT 1") != std::string::npos) {
        ++*static_cast<int*>(context);
    }
    return 0;
}
}

TEST(SearchExecutor, PageByNameMaterializesEachSearchAndReusesItForPaging) {
    MemoryDatabase first;
    int firstFolderCountQueries{};
    sqlite3_trace_v2(first.Raw(), SQLITE_TRACE_STMT, TraceSearchCacheBuild, &firstFolderCountQueries);

    wit::search::SqliteSearchExecutor executor(first.Raw());
    EXPECT_EQ(executor.PageByName(L"alpha", 0, 10).size(), 2u);
    EXPECT_EQ(executor.PageByName(L"alpha", 1, 10).size(), 1u);
    EXPECT_EQ(firstFolderCountQueries, 1);

    EXPECT_EQ(executor.PageByName(L"beta", 0, 10).size(), 1u);
    EXPECT_EQ(firstFolderCountQueries, 2);

    executor.SetDatabase(first.Raw());
    EXPECT_EQ(executor.PageByName(L"beta", 0, 10).size(), 1u);
    EXPECT_EQ(firstFolderCountQueries, 3);

    MemoryDatabase second;
    int secondFolderCountQueries{};
    sqlite3_trace_v2(second.Raw(), SQLITE_TRACE_STMT, TraceSearchCacheBuild, &secondFolderCountQueries);
    executor.SetDatabase(second.Raw());
    EXPECT_EQ(executor.PageByName(L"beta", 0, 10).size(), 1u);
    EXPECT_EQ(secondFolderCountQueries, 1);
}

TEST(SearchExecutor, PageByNameSupportsAsteriskWildcards) {
    MemoryDatabase database;
    wit::search::SqliteSearchExecutor executor(database.Raw());

    EXPECT_EQ(executor.CountByName(L"*"), 7);

    const auto textFiles = executor.PageByName(L"*.txt", 0, 10);
    ASSERT_EQ(textFiles.size(), 4u);
    EXPECT_EQ(textFiles[0].name, L"alpha-file.txt");
    EXPECT_EQ(textFiles[1].name, L"beta-file.txt");
    EXPECT_EQ(textFiles[2].name, L"item2.txt");
    EXPECT_EQ(textFiles[3].name, L"item10.txt");

    EXPECT_EQ(executor.CountByName(L"alpha*"), 2);
    EXPECT_EQ(executor.CountByName(L"alpha*txt"), 1);
    EXPECT_EQ(executor.CountByName(L"**"), 7);
    EXPECT_EQ(executor.CountByName(L"*file*"), 2);
    EXPECT_EQ(executor.CountByName(L"alpha"), 2) << "terms without wildcards remain substring searches";
    EXPECT_EQ(executor.CountByName(L"%"), 0) << "SQL LIKE metacharacters remain literal";
    EXPECT_EQ(executor.CountByName(L"_"), 0) << "SQL LIKE metacharacters remain literal";
}

TEST(SearchExecutor, AdvancedSearchFiltersWithBoundCriteria) {
    MemoryDatabase database;
    wit::search::SqliteSearchExecutor executor(database.Raw());

    const auto parsed = wit::search::ParseAdvancedSearchQuery(L"filename = \"alpha-file.txt\" and filesize >= \"40 bytes\"");
    ASSERT_TRUE(parsed.success);

    EXPECT_EQ(executor.CountAdvanced(parsed.expression), 1);
    const auto page = executor.PageAdvanced(parsed.expression, 0, 10);
    ASSERT_EQ(page.size(), 1u);
    EXPECT_EQ(page[0].name, L"alpha-file.txt");

    const auto folderParsed = wit::search::ParseAdvancedSearchQuery(L"filename = \"alpha-folder\" or filesize = \"43 bytes\"");
    ASSERT_TRUE(folderParsed.success);
    EXPECT_EQ(executor.CountAdvanced(folderParsed.expression), 2);
}
