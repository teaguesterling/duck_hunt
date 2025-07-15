#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Stylelint JSON output.
 * Handles CSS/SCSS style linting with file-based warning structure.
 */
class StylelintJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::STYLELINT_JSON; }
    std::string getName() const override { return "stylelint_json"; }
    int getPriority() const override { return 70; }  // Medium-high priority for CSS linting
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidStylelintJSON(const std::string& content) const;
};

} // namespace duckdb