#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for SpotBugs JSON output.
 * Handles Java static analysis with bug categories and priority levels.
 */
class SpotBugsJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "spotbugs_json"; }
    std::string getName() const override { return "spotbugs_json"; }
    int getPriority() const override { return 75; }  // Medium-high priority for Java analysis
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidSpotBugsJSON(const std::string& content) const;
};

} // namespace duckdb