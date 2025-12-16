#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Markdownlint JSON output.
 * Handles Markdown documentation linting with rule names and error ranges.
 */
class MarkdownlintJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::MARKDOWNLINT_JSON; }
    std::string getName() const override { return "markdownlint_json"; }
    int getPriority() const override { return 65; }  // Medium priority for documentation linting
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidMarkdownlintJSON(const std::string& content) const;
};

} // namespace duckdb