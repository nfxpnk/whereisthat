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
            "VALUES(1,1,1,'alpha-file.txt','txt',42,102,0),(2,1,1,'beta-file.txt','txt',43,103,0);");
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

int TraceFolderCount(unsigned int, void* context, void* statement, void*) {
    const auto sql = std::string(sqlite3_sql(static_cast<sqlite3_stmt*>(statement)));
    if (sql.find("SELECT COUNT(*) FROM folders WHERE name LIKE") != std::string::npos) {
        ++*static_cast<int*>(context);
    }
    return 0;
}
}

TEST(SearchExecutor, PageByNameCachesFolderCountPerSearchTermAndDatabase) {
    MemoryDatabase first;
    int firstFolderCountQueries{};
    sqlite3_trace_v2(first.Raw(), SQLITE_TRACE_STMT, TraceFolderCount, &firstFolderCountQueries);

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
    sqlite3_trace_v2(second.Raw(), SQLITE_TRACE_STMT, TraceFolderCount, &secondFolderCountQueries);
    executor.SetDatabase(second.Raw());
    EXPECT_EQ(executor.PageByName(L"beta", 0, 10).size(), 1u);
    EXPECT_EQ(secondFolderCountQueries, 1);
}
