#include "SQLiteStatement.h"
#include "../../third_party/sqlite/sqlite3.h"
#include <stdexcept>

namespace wit::storage {
SQLiteStatement::SQLiteStatement(sqlite3* db, const char* sql) {
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) throw std::runtime_error("sqlite prepare failed");
}

SQLiteStatement::~SQLiteStatement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

void SQLiteStatement::BindInt64(int index, long long value) {
    sqlite3_bind_int64(stmt_, index, value);
}

void SQLiteStatement::BindText(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void SQLiteStatement::BindNull(int index) {
    sqlite3_bind_null(stmt_, index);
}

bool SQLiteStatement::Step() {
    return sqlite3_step(stmt_) == SQLITE_ROW;
}

void SQLiteStatement::Reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}
}
