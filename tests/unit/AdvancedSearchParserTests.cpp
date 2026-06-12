#include <gtest/gtest.h>
#include <wit_search/AdvancedSearchParser.h>

#include <string>

namespace {
std::uint64_t ParseSizeOrZero(std::wstring_view text) {
    std::uint64_t bytes{};
    std::wstring error;
    EXPECT_TRUE(wit::search::TryParseFileSize(text, bytes, error));
    return bytes;
}

void ExpectInvalidSize(std::wstring_view text) {
    std::uint64_t bytes{};
    std::wstring error;
    EXPECT_FALSE(wit::search::TryParseFileSize(text, bytes, error));
    EXPECT_FALSE(error.empty());
}

void ExpectValidQuery(std::wstring_view query, std::size_t criteria, std::size_t logicalOperators) {
    const auto result = wit::search::ParseAdvancedSearchQuery(query);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.expression.criteria.size(), criteria);
    EXPECT_EQ(result.expression.logicalOperators.size(), logicalOperators);
}

void ExpectInvalidQuery(std::wstring_view query) {
    const auto result = wit::search::ParseAdvancedSearchQuery(query);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}
}

TEST(AdvancedSearchParser, ParsesHumanReadableFileSizes) {
    EXPECT_EQ(ParseSizeOrZero(L"102 KB"), 104448ull);
    EXPECT_EQ(ParseSizeOrZero(L"102KB"), 104448ull);
    EXPECT_EQ(ParseSizeOrZero(L"2 TB"), 2199023255552ull);
    EXPECT_EQ(ParseSizeOrZero(L"1.5 GB"), 1610612736ull);
    EXPECT_EQ(ParseSizeOrZero(L"500 bytes"), 500ull);
    EXPECT_EQ(ParseSizeOrZero(L"1 b"), 1ull);
}

TEST(AdvancedSearchParser, RejectsInvalidFileSizes) {
    ExpectInvalidSize(L"abc TB");
    ExpectInvalidSize(L"20 XB");
    ExpectInvalidSize(L"");
    ExpectInvalidSize(L"9223372036854775808 bytes");
}

TEST(AdvancedSearchParser, ParsesInitialQuerySyntax) {
    ExpectValidQuery(L"filename = \"test123\"", 1, 0);
    ExpectValidQuery(L"filesize >= \"20 TB\"", 1, 0);
    ExpectValidQuery(L"filename = \"test123\" and filesize >= \"20 TB\"", 2, 1);
    ExpectValidQuery(L"filename = \"a\" or filename = \"b\"", 2, 1);
    ExpectValidQuery(L"filename = \"a\" xor filesize < \"10 MB\"", 2, 1);
    ExpectValidQuery(L"FILENAME = \"a\" AND FileSize < \"10 mb\"", 2, 1);
}

TEST(AdvancedSearchParser, RejectsInvalidQueries) {
    ExpectInvalidQuery(L"filename \"test123\"");
    ExpectInvalidQuery(L"filename = test123");
    ExpectInvalidQuery(L"unknownfield = \"abc\"");
    ExpectInvalidQuery(L"filename = \"test123\" and");
    ExpectInvalidQuery(L"filename <= \"test123\"");
    ExpectInvalidQuery(L"filesize => \"20 TB\"");
    ExpectInvalidQuery(L"filesize >= \"20 XB\"");
    ExpectInvalidQuery(L"filesize >= \"abc TB\"");
}
