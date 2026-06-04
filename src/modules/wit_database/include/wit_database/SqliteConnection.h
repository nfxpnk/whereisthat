#pragma once
#include <string>

struct sqlite3;

namespace wit::storage {
class SqliteConnection {
public:
    SqliteConnection() = default;
    ~SqliteConnection();
    SqliteConnection(const SqliteConnection&) = delete;
    SqliteConnection& operator=(const SqliteConnection&) = delete;
    SqliteConnection(SqliteConnection&& other) noexcept;
    SqliteConnection& operator=(SqliteConnection&& other) noexcept;

    [[nodiscard]] bool Open(const std::wstring& path, int flags);
    [[nodiscard]] bool OpenMemory();
    void Close();
    bool IsOpen() const { return db_ != nullptr; }
    sqlite3* Raw() const { return db_; }
    [[nodiscard]] bool Exec(const char* sql);
    const std::wstring& Path() const { return path_; }
    int LastErrorCode() const { return lastErrorCode_; }
    const std::string& LastErrorMessage() const { return lastErrorMessage_; }

private:
    void CaptureLastError();

    sqlite3* db_{};
    std::wstring path_;
    int lastErrorCode_{};
    std::string lastErrorMessage_;
};
}
