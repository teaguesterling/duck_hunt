#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Ktlint JSON output.
 * Handles Kotlin code style validation with rule-based error reporting.
 */
class KtlintJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::KTLINT_JSON; }
    std::string getName() const override { return "ktlint_json"; }
    int getPriority() const override { return 70; }  // Medium-high priority for Kotlin linting
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidKtlintJSON(const std::string& content) const;
};

} // namespace duckdb