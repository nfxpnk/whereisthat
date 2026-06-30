#include <gtest/gtest.h>
#include <wit_database/SqliteFileListHelpers.h>
#include <wit_types/FileSort.h>

TEST(FileSortHelpers, MapsListColumnsToSharedFileSortColumns) {
    EXPECT_EQ(wit::core::FileSortColumnFromListColumn(0), wit::core::FileSortColumn::Name);
    EXPECT_EQ(wit::core::FileSortColumnFromListColumn(1), wit::core::FileSortColumn::Type);
    EXPECT_EQ(wit::core::FileSortColumnFromListColumn(2), wit::core::FileSortColumn::Size);
    EXPECT_EQ(wit::core::FileSortColumnFromListColumn(3), wit::core::FileSortColumn::Path);
    EXPECT_EQ(wit::core::FileSortColumnFromListColumn(4), wit::core::FileSortColumn::Modified);
    EXPECT_FALSE(wit::core::FileSortColumnFromListColumn(-1).has_value());
    EXPECT_FALSE(wit::core::FileSortColumnFromListColumn(5).has_value());
}

TEST(FileSortHelpers, MapsSharedFileSortColumnsToListColumns) {
    EXPECT_EQ(wit::core::ListColumnFromFileSortColumn(wit::core::FileSortColumn::Name), 0);
    EXPECT_EQ(wit::core::ListColumnFromFileSortColumn(wit::core::FileSortColumn::Type), 1);
    EXPECT_EQ(wit::core::ListColumnFromFileSortColumn(wit::core::FileSortColumn::Size), 2);
    EXPECT_EQ(wit::core::ListColumnFromFileSortColumn(wit::core::FileSortColumn::Path), 3);
    EXPECT_EQ(wit::core::ListColumnFromFileSortColumn(wit::core::FileSortColumn::Modified), 4);
}

TEST(FileSortHelpers, BuildsSharedSqlOrderExpressionsForEachColumn) {
    EXPECT_STREQ(wit::storage::FileEntryOrderExpression(wit::core::FileSortColumn::Name),
        "name COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::FileEntryOrderExpression(wit::core::FileSortColumn::Type),
        "sort_type COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::FileEntryOrderExpression(wit::core::FileSortColumn::Size), "size");
    EXPECT_STREQ(wit::storage::FileEntryOrderExpression(wit::core::FileSortColumn::Path),
        "parent_path COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::FileEntryOrderExpression(wit::core::FileSortColumn::Modified), "modified_at");

    EXPECT_EQ(wit::storage::FileEntryOrderBy({wit::core::FileSortColumn::Path, false}),
        "ORDER BY parent_path COLLATE WIN_NATURAL_NOCASE DESC, name COLLATE WIN_NATURAL_NOCASE ASC,is_directory DESC,id ASC ");
}

TEST(FileSortHelpers, NaturalComparisonIgnoresCaseAndSortsDigitsAsNumbers) {
    EXPECT_EQ(wit::storage::NaturalNoCaseCompareUtf8("Alpha", "alpha"), 0);
    EXPECT_LT(wit::storage::NaturalNoCaseCompareUtf8("item2.txt", "item10.txt"), 0);
}
