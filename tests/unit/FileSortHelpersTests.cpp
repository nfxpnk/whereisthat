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

TEST(FileSortHelpers, BuildsBrowserContentSqlOrderExpressionsForFoldersAndFiles) {
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Name, true), "c.name COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Name, false), "f.name COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Type, true), "c.entry_type COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Type, false), "f.extension COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Size, true), "c.content_size");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Size, false), "f.size");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Path, true),
        "(SELECT path FROM parent) COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Path, false),
        "(SELECT path FROM parent) COLLATE WIN_NATURAL_NOCASE");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Modified, true), "c.modified_at");
    EXPECT_STREQ(wit::storage::BrowserContentOrderExpression(wit::core::FileSortColumn::Modified, false), "f.modified_at");
}

TEST(FileSortHelpers, BuildsBrowserContentOrderByForFolderFirstRepositoryQueries) {
    EXPECT_EQ(wit::storage::BrowserContentOrderBy({wit::core::FileSortColumn::Name, true}, true),
        "ORDER BY c.name COLLATE WIN_NATURAL_NOCASE ASC,c.id ASC ");
    EXPECT_EQ(wit::storage::BrowserContentOrderBy({wit::core::FileSortColumn::Name, false}, false),
        "ORDER BY f.name COLLATE WIN_NATURAL_NOCASE DESC,f.id ASC ");
    EXPECT_EQ(wit::storage::BrowserContentOrderBy({wit::core::FileSortColumn::Path, false}, false),
        "ORDER BY (SELECT path FROM parent) COLLATE WIN_NATURAL_NOCASE DESC, f.name COLLATE WIN_NATURAL_NOCASE ASC,f.id ASC ");
    EXPECT_EQ(wit::storage::BrowserContentOrderBy({wit::core::FileSortColumn::Type, true}, true),
        "ORDER BY c.entry_type COLLATE WIN_NATURAL_NOCASE ASC, c.name COLLATE WIN_NATURAL_NOCASE ASC,c.id ASC ");
}
TEST(FileSortHelpers, NaturalComparisonIgnoresCaseAndSortsDigitsAsNumbers) {
    EXPECT_EQ(wit::storage::NaturalNoCaseCompareUtf8("Alpha", "alpha"), 0);
    EXPECT_LT(wit::storage::NaturalNoCaseCompareUtf8("item2.txt", "item10.txt"), 0);
}
