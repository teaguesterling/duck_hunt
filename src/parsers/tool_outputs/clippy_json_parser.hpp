#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Clippy JSON output (JSONL format).
 * Handles Rust linting with primary span detection and suggestions.
 */
class ClippyJSONParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "clippy_json";
	}
	std::string getName() const override {
		return "clippy_json";
	}
	int getPriority() const override {
		return 75;
	} // Medium-high priority for Rust linting
	std::string getCategory() const override {
		return "tool_output";
	}

private:
	bool isValidClippyJSON(const std::string &content) const;
};

} // namespace duckdb
