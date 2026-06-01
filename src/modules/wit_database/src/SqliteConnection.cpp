#include "wit_database/SqliteConnection.h"
#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"
#include <utility>

namespace wit::storage {
SqliteConnection::~SqliteConnection() {
    Close();
}

SqliteConnection::SqliteConnection(SqliteConnection&& other) noexcept
    : db_(std::exchange(other.db_, nullptr)), path_(std::move(other.path_)),
      lastErrorCode_(std::exchange(other.lastErrorCode_, 0)), lastErrorMessage_(std::move(other.lastErrorMessage_)) {}

SqliteConnection& SqliteConnection::operator=(SqliteConnection&& other) noexcept {
    if (this != &other) {
        Close();
        db_ = std::exchange(other.db_, nullptr);
        path_ = std::move(other.path_);
        lastErrorCode_ = std::exchange(other.lastErrorCode_, 0);
        lastErrorMessage_ = std::move(other.lastErrorMessage_);
    }
    return *this;
}

bool SqliteConnection::Open(const std::wstring& path, int flags) {
    Close();
    if (sqlite3_open_v2(wit::platform::ToUtf8(path).c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        CaptureLastError();
        Close();
        return false;
    }
    path_ = path;
    lastErrorCode_ = SQLITE_OK;
    lastErrorMessage_.clear();
    return true;
}

bool SqliteConnection::OpenMemory() {
    Close();
    if (sqlite3_open_v2(":memory:", &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        CaptureLastError();
        Close();
        return false;
    }
    lastErrorCode_ = SQLITE_OK;
    lastErrorMessage_.clear();
    return true;
}

void SqliteConnection::Close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    path_.clear();
}

bool SqliteConnection::Exec(const char* sql) {
    if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) == SQLITE_OK) {
        lastErrorCode_ = SQLITE_OK;
        lastErrorMessage_.clear();
        return true;
    }
    CaptureLastError();
    return false;
}

void SqliteConnection::CaptureLastError() {
    lastErrorCode_ = db_ ? sqlite3_errcode(db_) : SQLITE_MISUSE;
    const char* message = db_ ? sqlite3_errmsg(db_) : nullptr;
    lastErrorMessage_ = message ? message : std::string{};
}
}
