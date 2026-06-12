#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wit::search {

enum class AdvancedSearchField {
    Filename,
    Filesize
};

enum class AdvancedSearchOperator {
    Equal,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual
};

enum class AdvancedSearchLogicalOperator {
    And,
    Or,
    Xor
};

struct AdvancedSearchCriterion {
    AdvancedSearchField field{};
    AdvancedSearchOperator comparison{};
    std::wstring textValue;
    std::uint64_t sizeValue{};
};

struct AdvancedSearchExpression {
    std::vector<AdvancedSearchCriterion> criteria;
    std::vector<AdvancedSearchLogicalOperator> logicalOperators;
};

struct AdvancedSearchParseResult {
    bool success{};
    AdvancedSearchExpression expression;
    std::wstring error;
};

bool TryParseFileSize(std::wstring_view text, std::uint64_t& bytes, std::wstring& error);
AdvancedSearchParseResult ParseAdvancedSearchQuery(std::wstring_view query);

}
