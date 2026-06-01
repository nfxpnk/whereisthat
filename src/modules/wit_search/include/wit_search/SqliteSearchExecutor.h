#pragma once
#include <string>
#include <vector>
#include <wit_types/FileEntry.h>

struct sqlite3;

namespace wit::search {
class SqliteSearchExecutor {
public:
    explicit SqliteSearchExecutor(sqlite3* db);

    int CountByName(const std::wstring& nameTerm);
    std::vector<wit::core::FileEntry> PageByName(const std::wstring& nameTerm, int offset, int limit);

private:
    sqlite3* db_{};
};
}
