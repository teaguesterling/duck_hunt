#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for ShellCheck JSON output.
 * Handles shell script static analysis with SC#### error codes.
 */
class ShellCheckJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::SHELLCHECK_JSON; }
    std::string getName() const override { return "shellcheck_json"; }
    int getPriority() const override { return 75; }  // Medium-high priority for shell analysis
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidShellCheckJSON(const std::string& content) const;
};

} // namespace duckdb