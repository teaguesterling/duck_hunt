#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for tfsec JSON output.
 * Handles Terraform security scanning with rule IDs and severity levels.
 */
class TfsecJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "tfsec_json"; }
    std::string getName() const override { return "tfsec_json"; }
    int getPriority() const override { return 85; }  // High priority for security scanning
    std::string getCategory() const override { return "security_tool"; }

private:
    bool isValidTfsecJSON(const std::string& content) const;
};

} // namespace duckdb
