#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for PHPStan JSON output.
 * Handles PHP static analysis results with file-based message structure.
 */
class PHPStanJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "phpstan_json"; }
    std::string getName() const override { return "phpstan_json"; }
    int getPriority() const override { return 70; }  // Medium-high priority for PHP analysis
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidPHPStanJSON(const std::string& content) const;
};

} // namespace duckdb