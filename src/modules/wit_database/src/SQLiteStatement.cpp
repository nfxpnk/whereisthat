#include "wit_database/SQLiteStatement.h"
#include <wit_infra/Logging.h>
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"
#include <format>

namespace wit::storage {
SQLiteStatement::SQLiteStatement(sqlite3* db, const char* sql) {
    const int result = sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr);
    if (result == SQLITE_OK) return;

    const char* message = db ? sqlite3_errmsg(db) : "database is not open";
    WIT_LOG_ERROR(std::format(L"sqlite prepare failed code={} message='{}' sql='{}'",
        result,
        wit::platform::ToUtf16(message ? message : ""),
        wit::platform::ToUtf16(sql ? sql : "")));
    stmt_ = nullptr;
}

SQLiteStatement::~SQLiteStatement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

void SQLiteStatement::BindInt64(int index, long long value) {
    if (!stmt_) return;
    sqlite3_bind_int64(stmt_, index, value);
}

void SQLiteStatement::BindText(int index, const std::string& value) {
    if (!stmt_) return;
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void SQLiteStatement::BindNull(int index) {
    if (!stmt_) return;
    sqlite3_bind_null(stmt_, index);
}

bool SQLiteStatement::Step() {
    if (!stmt_) return false;
    return sqlite3_step(stmt_) == SQLITE_ROW;
}

void SQLiteStatement::Reset() {
    if (!stmt_) return;
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}
}

