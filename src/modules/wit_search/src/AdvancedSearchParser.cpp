#include "wit_search/AdvancedSearchParser.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>

namespace wit::search {
namespace {
enum class TokenKind {
    Identifier,
    Operator,
    QuotedString,
    EndOfInput,
    Invalid
};

struct Token {
    TokenKind kind{};
    std::wstring text;
};

struct FieldDescriptor {
    std::wstring_view name;
    AdvancedSearchField field;
    bool supportsEqual{};
    bool supportsOrdering{};
};

constexpr std::array FieldRegistry{
    FieldDescriptor{L"filename", AdvancedSearchField::Filename, true, false},
    FieldDescriptor{L"filesize", AdvancedSearchField::Filesize, true, true},
};

bool IsIdentifierStart(wchar_t ch) {
    return std::iswalpha(ch) || ch == L'_';
}

bool IsIdentifierPart(wchar_t ch) {
    return std::iswalnum(ch) || ch == L'_';
}

std::wstring ToLower(std::wstring_view text) {
    std::wstring lowered(text);
    std::ranges::transform(lowered, lowered.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return lowered;
}

std::wstring Trim(std::wstring_view text) {
    const auto first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring_view::npos) return {};
    const auto last = text.find_last_not_of(L" \t\r\n");
    return std::wstring(text.substr(first, last - first + 1));
}

std::wstring OperatorText(AdvancedSearchOperator op) {
    switch (op) {
    case AdvancedSearchOperator::Equal: return L"=";
    case AdvancedSearchOperator::Less: return L"<";
    case AdvancedSearchOperator::LessOrEqual: return L"<=";
    case AdvancedSearchOperator::Greater: return L">";
    case AdvancedSearchOperator::GreaterOrEqual: return L">=";
    default: return L"?";
    }
}

class Tokenizer {
public:
    explicit Tokenizer(std::wstring_view input) : input_(input) {}

    Token Next() {
        SkipWhitespace();
        if (position_ >= input_.size()) return {TokenKind::EndOfInput, {}};

        const wchar_t ch = input_[position_];
        if (IsIdentifierStart(ch)) return ReadIdentifier();
        if (ch == L'"') return ReadQuotedString();
        if (ch == L'=' || ch == L'<' || ch == L'>') return ReadOperator();

        ++position_;
        return {TokenKind::Invalid, std::wstring(1, ch)};
    }

private:
    void SkipWhitespace() {
        while (position_ < input_.size() && std::iswspace(input_[position_])) ++position_;
    }

    Token ReadIdentifier() {
        const auto start = position_++;
        while (position_ < input_.size() && IsIdentifierPart(input_[position_])) ++position_;
        return {TokenKind::Identifier, std::wstring(input_.substr(start, position_ - start))};
    }

    Token ReadQuotedString() {
        ++position_;
        const auto start = position_;
        while (position_ < input_.size() && input_[position_] != L'"') ++position_;
        if (position_ >= input_.size()) return {TokenKind::Invalid, L"unterminated quoted value"};
        const auto value = std::wstring(input_.substr(start, position_ - start));
        ++position_;
        return {TokenKind::QuotedString, value};
    }

    Token ReadOperator() {
        const wchar_t first = input_[position_++];
        if (position_ < input_.size() && input_[position_] == L'=') {
            ++position_;
            return {TokenKind::Operator, std::wstring{first, L'='}};
        }
        return {TokenKind::Operator, std::wstring(1, first)};
    }

    std::wstring_view input_;
    std::size_t position_{};
};

std::optional<FieldDescriptor> LookupField(std::wstring_view name) {
    const auto lowered = ToLower(name);
    const auto found = std::ranges::find_if(FieldRegistry,
        [&lowered](const FieldDescriptor& descriptor) { return descriptor.name == lowered; });
    if (found == FieldRegistry.end()) return std::nullopt;
    return *found;
}

std::optional<AdvancedSearchOperator> ParseOperator(std::wstring_view text) {
    if (text == L"=") return AdvancedSearchOperator::Equal;
    if (text == L"<") return AdvancedSearchOperator::Less;
    if (text == L"<=") return AdvancedSearchOperator::LessOrEqual;
    if (text == L">") return AdvancedSearchOperator::Greater;
    if (text == L">=") return AdvancedSearchOperator::GreaterOrEqual;
    return std::nullopt;
}

std::optional<AdvancedSearchLogicalOperator> ParseLogicalOperator(std::wstring_view text) {
    const auto lowered = ToLower(text);
    if (lowered == L"and") return AdvancedSearchLogicalOperator::And;
    if (lowered == L"or") return AdvancedSearchLogicalOperator::Or;
    if (lowered == L"xor") return AdvancedSearchLogicalOperator::Xor;
    return std::nullopt;
}

bool IsOperatorSupported(const FieldDescriptor& field, AdvancedSearchOperator op) {
    return op == AdvancedSearchOperator::Equal ? field.supportsEqual : field.supportsOrdering;
}

class Parser {
public:
    explicit Parser(std::wstring_view input) : tokenizer_(input) {}

    AdvancedSearchParseResult Parse() {
        AdvancedSearchParseResult result;
        auto first = ParseCriterion(result.error);
        if (!first) return result;
        result.expression.criteria.push_back(std::move(*first));

        // This first implementation intentionally parses a flat left-to-right
        // sequence. Parentheses and precedence can be layered in here later.
        for (;;) {
            Token token = tokenizer_.Next();
            if (token.kind == TokenKind::EndOfInput) {
                result.success = true;
                return result;
            }
            if (token.kind != TokenKind::Identifier) {
                result.error = L"Expected logical operator between criteria.";
                return result;
            }

            auto logical = ParseLogicalOperator(token.text);
            if (!logical) {
                result.error = L"Expected logical operator between criteria.";
                return result;
            }

            auto criterion = ParseCriterion(result.error);
            if (!criterion) {
                if (result.error.empty()) result.error = L"Query cannot end with a logical operator.";
                return result;
            }

            result.expression.logicalOperators.push_back(*logical);
            result.expression.criteria.push_back(std::move(*criterion));
        }
    }

private:
    std::optional<AdvancedSearchCriterion> ParseCriterion(std::wstring& error) {
        Token fieldToken = tokenizer_.Next();
        if (fieldToken.kind == TokenKind::EndOfInput) return std::nullopt;
        if (fieldToken.kind != TokenKind::Identifier) {
            error = L"Expected field name.";
            return std::nullopt;
        }

        const auto field = LookupField(fieldToken.text);
        if (!field) {
            error = L"Unknown field: " + fieldToken.text + L".";
            return std::nullopt;
        }

        Token operatorToken = tokenizer_.Next();
        if (operatorToken.kind != TokenKind::Operator) {
            error = L"Expected comparison operator after field name.";
            return std::nullopt;
        }

        const auto comparison = ParseOperator(operatorToken.text);
        if (!comparison || !IsOperatorSupported(*field, *comparison)) {
            error = L"Operator " + operatorToken.text + L" is not supported for " +
                std::wstring(field->name) + L".";
            return std::nullopt;
        }

        Token valueToken = tokenizer_.Next();
        if (valueToken.kind != TokenKind::QuotedString) {
            error = L"Expected quoted value.";
            return std::nullopt;
        }

        AdvancedSearchCriterion criterion;
        criterion.field = field->field;
        criterion.comparison = *comparison;
        if (criterion.field == AdvancedSearchField::Filename) {
            criterion.textValue = valueToken.text;
            return criterion;
        }

        std::uint64_t bytes{};
        if (!TryParseFileSize(valueToken.text, bytes, error)) return std::nullopt;
        criterion.sizeValue = bytes;
        return criterion;
    }

    Tokenizer tokenizer_;
};
}

bool TryParseFileSize(std::wstring_view text, std::uint64_t& bytes, std::wstring& error) {
    const auto trimmed = Trim(text);
    if (trimmed.empty()) {
        error = L"Expected filesize value.";
        return false;
    }

    std::size_t position{};
    bool sawDigit = false;
    bool sawDecimal = false;
    while (position < trimmed.size()) {
        const wchar_t ch = trimmed[position];
        if (std::iswdigit(ch)) {
            sawDigit = true;
            ++position;
            continue;
        }
        if (ch == L'.' && !sawDecimal) {
            sawDecimal = true;
            ++position;
            continue;
        }
        break;
    }
    if (!sawDigit) {
        error = L"Invalid filesize value.";
        return false;
    }

    const auto numberText = trimmed.substr(0, position);
    while (position < trimmed.size() && std::iswspace(trimmed[position])) ++position;
    const auto unitText = ToLower(std::wstring_view(trimmed).substr(position));
    if (unitText.empty()) {
        error = L"Expected filesize unit.";
        return false;
    }

    std::uint64_t multiplier{};
    if (unitText == L"bytes" || unitText == L"byte" || unitText == L"b") multiplier = 1ull;
    else if (unitText == L"kb") multiplier = 1024ull;
    else if (unitText == L"mb") multiplier = 1024ull * 1024ull;
    else if (unitText == L"gb") multiplier = 1024ull * 1024ull * 1024ull;
    else if (unitText == L"tb") multiplier = 1024ull * 1024ull * 1024ull * 1024ull;
    else if (unitText == L"pb") multiplier = 1024ull * 1024ull * 1024ull * 1024ull * 1024ull;
    else if (unitText == L"eb") multiplier = 1024ull * 1024ull * 1024ull * 1024ull * 1024ull * 1024ull;
    else {
        error = L"Invalid filesize unit: " + std::wstring(unitText) + L".";
        return false;
    }

    try {
        std::size_t parsed{};
        const long double value = std::stold(numberText, &parsed);
        if (parsed != numberText.size() || value < 0.0L) {
            error = L"Invalid filesize value.";
            return false;
        }
        const long double byteValue = value * static_cast<long double>(multiplier);
        const long double firstUnsafeSignedValue = 9223372036854775808.0L;
        if (byteValue >= firstUnsafeSignedValue) {
            error = L"File size value is too large.";
            return false;
        }
        bytes = static_cast<std::uint64_t>(byteValue);
        return true;
    } catch (...) {
        error = L"Invalid filesize value.";
        return false;
    }
}

AdvancedSearchParseResult ParseAdvancedSearchQuery(std::wstring_view query) {
    if (Trim(query).empty()) {
        return {.success = false, .expression = {}, .error = L"Please enter search criteria."};
    }
    return Parser(query).Parse();
}

}
