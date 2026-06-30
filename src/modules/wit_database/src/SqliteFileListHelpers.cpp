#include "wit_database/SqliteFileListHelpers.h"

#include <wit_infra/Win32Helpers.h>
#include "third_party/sqlite/sqlite3.h"

#include <Windows.h>
#include <string_view>

namespace wit::storage {
namespace {
int NaturalNoCaseCollation(void*, int leftBytes, const void* leftValue, int rightBytes, const void* rightValue) {
    const std::string_view leftUtf8(static_cast<const char*>(leftValue), static_cast<std::size_t>(leftBytes));
    const std::string_view rightUtf8(static_cast<const char*>(rightValue), static_cast<std::size_t>(rightBytes));
    return NaturalNoCaseCompareUtf8(leftUtf8, rightUtf8);
}
}

void EnsureNaturalNoCaseCollation(sqlite3* db) {
    if (!db) return;
    sqlite3_create_collation_v2(db, "WIN_NATURAL_NOCASE", SQLITE_UTF8, nullptr,
        NaturalNoCaseCollation, nullptr);
}

int NaturalNoCaseCompareUtf8(std::string_view leftUtf8, std::string_view rightUtf8) {
    const auto left = wit::platform::ToUtf16(std::string(leftUtf8));
    const auto right = wit::platform::ToUtf16(std::string(rightUtf8));
    const int result = CompareStringEx(LOCALE_NAME_USER_DEFAULT,
        LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS,
        left.c_str(), static_cast<int>(left.size()),
        right.c_str(), static_cast<int>(right.size()),
        nullptr, nullptr, 0);
    if (result == CSTR_LESS_THAN) return -1;
    if (result == CSTR_GREATER_THAN) return 1;
    return 0;
}

const char* FileEntryOrderExpression(wit::core::FileSortColumn column) {
    switch (column) {
    case wit::core::FileSortColumn::Type:
        return "sort_type COLLATE WIN_NATURAL_NOCASE";
    case wit::core::FileSortColumn::Size:
        return "size";
    case wit::core::FileSortColumn::Path:
        return "parent_path COLLATE WIN_NATURAL_NOCASE";
    case wit::core::FileSortColumn::Modified:
        return "modified_at";
    case wit::core::FileSortColumn::Name:
    default:
        return "name COLLATE WIN_NATURAL_NOCASE";
    }
}

std::string FileEntryOrderBy(wit::core::FileSort sort) {
    std::string order{"ORDER BY "};
    order += FileEntryOrderExpression(sort.column);
    order += sort.ascending ? " ASC," : " DESC,";
    order += " name COLLATE WIN_NATURAL_NOCASE ASC,is_directory DESC,id ASC ";
    return order;
}

}
