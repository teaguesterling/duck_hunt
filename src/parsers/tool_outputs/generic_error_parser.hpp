#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Generic fallback parser for error and warning messages without file:line prefixes.
 * Catches patterns like:
 *   - error: message
 *   - ERROR: message
 *   - Error: message
 *   - [ERROR] message
 *   - [FAIL] message
 *   - [FAILED] message
 *   - warning: message
 *   - WARNING: message
 *   - [WARNING] message
 *   - [WARN] message
 *
 * This is a VERY_LOW priority parser (10) - only used when no other parser matches.
 */
class GenericErrorParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "generic_error";
	}
	std::string getName() const override {
		return "generic";
	}
	std::string getDescription() const override {
		return "Generic error/warning message fallback parser";
	}
	int getPriority() const override {
		return 10;
	} // VERY_LOW - last resort fallback
	std::string getCategory() const override {
		return "tool_output";
	}

private:
	// Check if a line contains an error/warning pattern
	// Returns: 0 = no match, 1 = error, 2 = warning, 3 = info/other
	int classifyLine(const std::string &line, std::string &message) const;
};

} // namespace duckdb
