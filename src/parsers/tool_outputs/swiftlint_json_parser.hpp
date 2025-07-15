#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for SwiftLint JSON output.
 * Handles Swift code style and quality analysis results.
 */
class SwiftLintJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::SWIFTLINT_JSON; }
    std::string getName() const override { return "swiftlint_json"; }
    int getPriority() const override { return 70; }  // Medium-high priority for Swift linting
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidSwiftLintJSON(const std::string& content) const;
};

} // namespace duckdb