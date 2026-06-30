#pragma once

#include <wit_types/FileSort.h>

#include <string>
#include <string_view>

struct sqlite3;

namespace wit::storage {

void EnsureNaturalNoCaseCollation(sqlite3* db);

[[nodiscard]] const char* FileEntryOrderExpression(wit::core::FileSortColumn column);
[[nodiscard]] std::string FileEntryOrderBy(wit::core::FileSort sort);
[[nodiscard]] int NaturalNoCaseCompareUtf8(std::string_view leftUtf8, std::string_view rightUtf8);

}
