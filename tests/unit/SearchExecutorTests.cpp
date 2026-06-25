#include <gtest/gtest.h>
#include <wit_search/SqliteSearchExecutor.h>
#include <sqlite3.h>
#include <chrono>
#include <filesystem>
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
    void Execute(const char* sql) { Exec(sql); }

private:
    void Exec(const char* sql) {
        ASSERT_EQ(sqlite3_exec(db_, sql, nullptr, nullptr, nullptr), SQLITE_OK);
    }

    sqlite3* db_{};
};

class FileDatabase {
public:
    FileDatabase() {
        path_ = std::filesystem::temp_directory_path() /
            ("wit-search-generation-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite");
        sqlite3_open(path_.string().c_str(), &db_);
        Execute("CREATE TABLE folders(id INTEGER PRIMARY KEY,disk_id INTEGER,parent_folder_id INTEGER,path TEXT,"
            "name TEXT,content_size INTEGER,modified_at INTEGER,attributes INTEGER,entry_type TEXT);"
            "CREATE TABLE files(id INTEGER PRIMARY KEY,disk_id INTEGER,folder_id INTEGER,name TEXT,extension TEXT,"
            "size INTEGER,modified_at INTEGER,attributes INTEGER);"
            "INSERT INTO folders(id,disk_id,parent_folder_id,path,name,content_size,modified_at,attributes,entry_type) "
            "VALUES(1,1,NULL,'C:\\\\','alpha-folder',0,100,0,'directory');"
            "INSERT INTO files(id,disk_id,folder_id,name,extension,size,modified_at,attributes) "
            "VALUES(1,1,1,'alpha-file.txt','txt',42,102,0);");
    }

    ~FileDatabase() {
        if (db_) sqlite3_close(db_);
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    sqlite3* Raw() const { return db_; }

    void Execute(const char* sql) {
        ASSERT_EQ(sqlite3_exec(db_, sql, nullptr, nullptr, nullptr), SQLITE_OK);
    }

private:
    std::filesystem::path path_;
    sqlite3* db_{};
};

int TraceSearchCacheBuild(unsigned int, void* context, void* statement, void*) {
    const auto sql = std::string(sqlite3_sql(static_cast<sqlite3_stmt*>(statement)));
    if (sql.find("INSERT INTO wit_search_page_cache(") != std::string::npos &&
        sql.find("FROM folders c") != std::string::npos) {
        ++*static_cast<int*>(context);
    }
    return 0;
}
int DenySearchCacheReads(void*, int action, const char* table, const char*, const char*, const char*) {
    return action == SQLITE_READ && table && std::string_view(table) == "wit_search_page_cache"
        ? SQLITE_DENY : SQLITE_OK;
}

int DenyFileTableReads(void*, int action, const char* table, const char*, const char*, const char*) {
    return action == SQLITE_READ && table && std::string_view(table) == "files"
        ? SQLITE_DENY : SQLITE_OK;
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

TEST(SearchExecutor, PageByNameCanMatchCaseSensitively) {
    MemoryDatabase database;
    database.Execute("UPDATE files SET name='Alpha-file.txt' WHERE id=1;");
    wit::search::SqliteSearchExecutor executor(database.Raw());

    EXPECT_EQ(executor.CountByName(L"alpha"), 2);
    EXPECT_EQ(executor.CountByName(L"alpha", true), 1);
    EXPECT_EQ(executor.CountByName(L"Alpha", true), 1);

    const auto exactCase = executor.PageByName(L"Alpha*", 0, 10, {}, true);
    ASSERT_EQ(exactCase.size(), 1u);
    EXPECT_EQ(exactCase[0].name, L"Alpha-file.txt");
    EXPECT_TRUE(executor.PageByName(L"alpha*txt", 0, 10, {}, true).empty());
}

TEST(SearchExecutor, PageByNameSortsFoldersAndFilesTogetherBySize) {
    MemoryDatabase database;
    database.Execute("UPDATE folders SET content_size=44 WHERE id=1;");
    wit::search::SqliteSearchExecutor executor(database.Raw());

    const wit::core::FileSort sort{wit::core::FileSortColumn::Size, true};
    const auto entries = executor.PageByName(L"*", 0, 10, sort);

    ASSERT_EQ(entries.size(), 7u);
    EXPECT_EQ(entries[0].name, L"child");
    EXPECT_EQ(entries[1].name, L"alpha-file.txt");
    EXPECT_EQ(entries[2].name, L"beta-file.txt");
    EXPECT_EQ(entries[3].name, L"alpha-folder");
    EXPECT_EQ(entries[4].name, L"notes.txt.bak");
    EXPECT_EQ(entries[5].name, L"item10.txt");
    EXPECT_EQ(entries[6].name, L"item2.txt");

    const wit::core::FileSort descending{wit::core::FileSortColumn::Size, false};
    const auto reversed = executor.PageByName(L"*", 0, 10, descending);
    ASSERT_EQ(reversed.size(), 7u);
    EXPECT_EQ(reversed[0].name, L"item2.txt");
    EXPECT_EQ(reversed[1].name, L"item10.txt");
    EXPECT_EQ(reversed[2].name, L"alpha-folder");
    EXPECT_EQ(reversed[3].name, L"notes.txt.bak");
    EXPECT_EQ(reversed[4].name, L"beta-file.txt");
    EXPECT_EQ(reversed[5].name, L"alpha-file.txt");
    EXPECT_EQ(reversed[6].name, L"child");
}
TEST(SearchExecutor, PageByNameInvalidatesMaterializedResultsAfterMutation) {
    MemoryDatabase database;
    wit::search::SqliteSearchExecutor executor(database.Raw());

    ASSERT_EQ(executor.PageByName(L"alpha", 0, 10).size(), 2u);
    database.Execute(
        "INSERT INTO files(id,disk_id,folder_id,name,extension,size,modified_at,attributes) "
        "VALUES(6,1,1,'alpha-new.txt','txt',47,107,0);");

    const auto refreshed = executor.PageByName(L"alpha", 0, 10);
    ASSERT_EQ(refreshed.size(), 3u);
    EXPECT_EQ(refreshed[2].name, L"alpha-new.txt");
}

TEST(SearchExecutor, PageByNameInvalidatesDedicatedCacheAfterExternalMutation) {
    FileDatabase database;
    wit::search::SqliteSearchExecutor executor(database.Raw());

    ASSERT_EQ(executor.PageByName(L"alpha", 0, 10).size(), 2u);
    database.Execute(
        "INSERT INTO files(id,disk_id,folder_id,name,extension,size,modified_at,attributes) "
        "VALUES(2,1,1,'alpha-new.txt','txt',43,103,0);");

    const auto refreshed = executor.PageByName(L"alpha", 0, 10);
    ASSERT_EQ(refreshed.size(), 3u);
    EXPECT_EQ(refreshed[2].name, L"alpha-new.txt");
}

TEST(SearchExecutor, PreparedSearchTotalMatchesItsMaterializedSnapshot) {
    FileDatabase database;
    wit::search::SqliteSearchExecutor executor(database.Raw());

    const auto prepared = executor.PrepareByName(L"alpha", 1);
    EXPECT_EQ(prepared.total, 2);
    ASSERT_EQ(prepared.entries.size(), 1u);
    EXPECT_EQ(prepared.entries[0].name, L"alpha-file.txt");

    database.Execute(
        "INSERT INTO files(id,disk_id,folder_id,name,extension,size,modified_at,attributes) "
        "VALUES(2,1,1,'alpha-new.txt','txt',43,103,0);");
    EXPECT_TRUE(executor.PageByName(L"alpha", 2, 1).empty())
        << "paging must remain pinned to the prepared two-item snapshot";

    const auto refreshed = executor.PrepareByName(L"alpha", 1);
    EXPECT_EQ(refreshed.total, 3);
}

TEST(SearchExecutor, PreparedSearchKeepsDeletedAndUpdatedRowsStableWhilePaging) {
    FileDatabase database;
    wit::search::SqliteSearchExecutor executor(database.Raw());

    const auto prepared = executor.PrepareByName(L"alpha", 1);
    ASSERT_EQ(prepared.total, 2);
    database.Execute("UPDATE files SET name='changed.txt' WHERE id=1;");

    const auto firstPage = executor.PageByName(L"alpha", 0, 1);
    ASSERT_EQ(firstPage.size(), 1u);
    EXPECT_EQ(firstPage[0].name, L"alpha-file.txt");

    database.Execute("DELETE FROM files WHERE id=1;");
    const auto sameFirstPage = executor.PageByName(L"alpha", 0, 1);
    ASSERT_EQ(sameFirstPage.size(), 1u);
    EXPECT_EQ(sameFirstPage[0].name, L"alpha-file.txt");
}

TEST(SearchExecutor, PageFailureReportsAnError) {
    sqlite3* database{};
    ASSERT_EQ(sqlite3_open(":memory:", &database), SQLITE_OK);
    {
        wit::search::SqliteSearchExecutor executor(database);
        EXPECT_TRUE(executor.PageByName(L"*", 0, 10).empty());
        EXPECT_FALSE(executor.LastErrorMessage().empty());
    }
    sqlite3_close(database);
}

TEST(SearchExecutor, CachedPageReadFailureReportsAnErrorAndInvalidatesTheCache) {
    MemoryDatabase database;
    int cacheBuilds{};
    sqlite3_trace_v2(database.Raw(), SQLITE_TRACE_STMT, TraceSearchCacheBuild, &cacheBuilds);
    wit::search::SqliteSearchExecutor executor(database.Raw());

    ASSERT_EQ(executor.PageByName(L"alpha", 0, 10).size(), 2u);
    ASSERT_EQ(cacheBuilds, 1);

    sqlite3_set_authorizer(database.Raw(), DenySearchCacheReads, nullptr);
    EXPECT_TRUE(executor.PageByName(L"alpha", 0, 10).empty());
    EXPECT_FALSE(executor.LastErrorMessage().empty());

    sqlite3_set_authorizer(database.Raw(), nullptr, nullptr);
    EXPECT_EQ(executor.PageByName(L"alpha", 0, 10).size(), 2u);
    EXPECT_EQ(cacheBuilds, 2);
    EXPECT_TRUE(executor.LastErrorMessage().empty());
}

TEST(SearchExecutor, AdvancedCountReportsFailureFromTheFileQuery) {
    MemoryDatabase database;
    wit::search::SqliteSearchExecutor executor(database.Raw());
    const auto parsed = wit::search::ParseAdvancedSearchQuery(L"filename = \"alpha-file.txt\"");
    ASSERT_TRUE(parsed.success);

    sqlite3_set_authorizer(database.Raw(), DenyFileTableReads, nullptr);
    EXPECT_EQ(executor.CountAdvanced(parsed.expression), 0);
    EXPECT_FALSE(executor.LastErrorMessage().empty());
    sqlite3_set_authorizer(database.Raw(), nullptr, nullptr);
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
