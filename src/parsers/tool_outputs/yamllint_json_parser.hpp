#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Yamllint JSON output.
 * Handles YAML configuration file linting with rule-based validation.
 */
class YamllintJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::YAMLLINT_JSON; }
    std::string getName() const override { return "yamllint_json"; }
    int getPriority() const override { return 65; }  // Medium priority for config linting
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidYamllintJSON(const std::string& content) const;
};

} // namespace duckdb