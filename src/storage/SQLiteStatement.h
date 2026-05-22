#pragma once
#include <string>
struct sqlite3;
struct sqlite3_stmt;

namespace wit::storage {
class SQLiteStatement {
public:
    SQLiteStatement(sqlite3* db, const char* sql);
    ~SQLiteStatement();
    SQLiteStatement(const SQLiteStatement&) = delete;
    SQLiteStatement& operator=(const SQLiteStatement&) = delete;
    void BindInt64(int index, long long value);
    void BindText(int index, const std::string& value);
    bool Step();
    void Reset();
    sqlite3_stmt* Raw() const { return stmt_; }
private:
    sqlite3_stmt* stmt_{};
};
}
