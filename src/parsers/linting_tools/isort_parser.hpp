#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for isort Python import sorter output.
 * Handles diff output, "Fixing" messages, and summary statistics.
 *
 * isort is a Python utility to sort imports alphabetically and
 * automatically separated into sections. Output formats include:
 * - Diff mode: shows unified diff of import changes
 * - Fix mode: "Fixing <file>" messages
 * - Check mode: "ERROR: ... found an import in the wrong position"
 * - Summary: "Skipped X files"
 */
class IsortParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "isort_text";
	}
	std::string getName() const override {
		return "isort";
	}
	int getPriority() const override {
		return 75;
	} // Medium-high priority for formatting
	std::string getCategory() const override {
		return "linting_tool";
	}
};

} // namespace duckdb
