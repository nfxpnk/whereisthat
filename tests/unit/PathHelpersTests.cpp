#include <gtest/gtest.h>
#include <wit_infra/PathHelpers.h>

#include <filesystem>

TEST(PathHelpers, JoinsParentAndChildPaths) {
    EXPECT_EQ(wit::platform::Join(L"C:\\root", L"child"), L"C:\\root\\child");
    EXPECT_EQ(wit::platform::Join(L"C:\\root\\", L"child"), L"C:\\root\\child");
    EXPECT_EQ(wit::platform::Join(L"C:/root", L"child"), L"C:/root\\child");
    EXPECT_EQ(wit::platform::Join(L"", L"child"), L"child");
}

TEST(PathHelpers, ExtractsParentDirectory) {
    EXPECT_EQ(wit::platform::ParentDirectory(L"C:\\root\\child.txt"), L"C:\\root");
    EXPECT_EQ(wit::platform::ParentDirectory(L"C:\\root\\folder\\"), L"C:\\root\\folder");
    EXPECT_EQ(wit::platform::ParentDirectory(L"child.txt"), L"");
}

TEST(PathHelpers, ExtractsFileExtensionWithoutLeadingDot) {
    EXPECT_EQ(wit::platform::FileExtension(L"file"), L"");
    EXPECT_EQ(wit::platform::FileExtension(L"file."), L"");
    EXPECT_EQ(wit::platform::FileExtension(L"file.txt"), L"txt");
    EXPECT_EQ(wit::platform::FileExtension(L"archive.tar.gz"), L"gz");
    EXPECT_EQ(wit::platform::FileExtension(L".gitignore"), L"");
}

TEST(PathHelpers, BuildsDisplayNameForPath) {
    EXPECT_EQ(wit::platform::DisplayNameForPath(std::filesystem::path(L"C:\\root\\file.txt")), L"file.txt");
    EXPECT_EQ(wit::platform::DisplayNameForPath(std::filesystem::path(L"C:\\root\\folder\\")), L"folder");
    EXPECT_EQ(wit::platform::DisplayNameForPath(std::filesystem::path(L"C:\\")), L"C:\\");
}

TEST(PathHelpers, EnsuresLongPathPrefixForWindowsPaths) {
    EXPECT_EQ(wit::platform::EnsureLongPathPrefix(L"\\\\?\\C:\\root\\file.txt"), L"\\\\?\\C:\\root\\file.txt");
    EXPECT_EQ(wit::platform::EnsureLongPathPrefix(L"\\\\server\\share\\file.txt"),
        L"\\\\?\\UNC\\server\\share\\file.txt");
    EXPECT_EQ(wit::platform::EnsureLongPathPrefix(L"C:\\root\\file.txt"), L"\\\\?\\C:\\root\\file.txt");
    EXPECT_EQ(wit::platform::EnsureLongPathPrefix(L"C:/root/file.txt"), L"\\\\?\\C:\\root\\file.txt");
    EXPECT_EQ(wit::platform::EnsureLongPathPrefix(L"relative\\file.txt"), L"relative\\file.txt");
    EXPECT_EQ(wit::platform::EnsureLongPathPrefix(L"\\\\.\\PhysicalDrive0"), L"\\\\.\\PhysicalDrive0");
}
