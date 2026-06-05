#include <gtest/gtest.h>

#include "wit_gui/FileListPane.h"

namespace {

wit::ui::FileListView MakeList(int total) {
    wit::ui::FileListView list;
    list.total = total;
    return list;
}

}

TEST(FileListSelection, PlainSelectionReplacesPreviousSelection) {
    auto list = MakeList(100);

    list.SetSelectionRange(2, 2, true);
    list.SetSelectionRange(5, 5, true);
    EXPECT_EQ(list.SelectedCount(), 2);

    list.SelectOnly(7);

    EXPECT_EQ(list.SelectedCount(), 1);
    EXPECT_FALSE(list.IsSelected(2));
    EXPECT_FALSE(list.IsSelected(5));
    EXPECT_TRUE(list.IsSelected(7));
}

TEST(FileListSelection, PlainSelectionAfterSelectAllClearsAllSelectedMode) {
    auto list = MakeList(500000);

    list.SelectAllItems();
    EXPECT_TRUE(list.AllItemsSelected());
    EXPECT_EQ(list.SelectedCount(), 500000);

    list.SelectOnly(1234);

    EXPECT_FALSE(list.AllItemsSelected());
    EXPECT_EQ(list.SelectedCount(), 1);
    EXPECT_FALSE(list.IsSelected(0));
    EXPECT_TRUE(list.IsSelected(1234));
    EXPECT_FALSE(list.IsSelected(499999));
}

TEST(FileListSelection, SelectAllSupportsDeselectedExceptions) {
    auto list = MakeList(50);

    list.SelectAllItems();
    list.SetSelectionRange(10, 12, false);

    EXPECT_EQ(list.SelectedCount(), 47);
    EXPECT_TRUE(list.IsSelected(9));
    EXPECT_FALSE(list.IsSelected(10));
    EXPECT_FALSE(list.IsSelected(11));
    EXPECT_FALSE(list.IsSelected(12));
    EXPECT_TRUE(list.IsSelected(13));
}
