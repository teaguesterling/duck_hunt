#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Bandit JSON output.
 * Handles Python security analysis with CWE mappings and severity levels.
 */
class BanditJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "bandit_json"; }
    std::string getName() const override { return "bandit_json"; }
    int getPriority() const override { return 80; }  // High priority for security analysis
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidBanditJSON(const std::string& content) const;
};

} // namespace duckdb