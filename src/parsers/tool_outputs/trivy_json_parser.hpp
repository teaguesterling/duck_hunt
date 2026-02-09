#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Trivy JSON output.
 * Handles container and dependency vulnerability scanning with CVE/CVSS data.
 */
class TrivyJSONParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "trivy_json";
	}
	std::string getName() const override {
		return "trivy_json";
	}
	int getPriority() const override {
		return 85;
	} // High priority for security scanning
	std::string getCategory() const override {
		return "security_tool";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    // Only match commands with explicit JSON format flag
		    CommandPattern::Like("trivy%-f json%"),
		    CommandPattern::Like("trivy%--format json%"),
		    CommandPattern::Like("trivy%--format=json%"),
		    CommandPattern::Regexp("trivy.*(-f|--format)[= ]?json"),
		};
	}

private:
	bool isValidTrivyJSON(const std::string &content) const;
};

} // namespace duckdb
