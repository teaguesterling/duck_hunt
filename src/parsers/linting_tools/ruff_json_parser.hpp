#pragma once

#include "parsers/base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for Ruff Python linter JSON output (--output-format=json).
 */
class RuffJsonParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "ruff_json";
	}
	std::string getName() const override {
		return "ruff_json";
	}
	std::string getDescription() const override {
		return "Ruff Python linter JSON output (--output-format=json)";
	}
	int getPriority() const override {
		return 100; // VERY_HIGH - JSON format is unambiguous
	}
	std::string getCategory() const override {
		return "linting_tool";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Like("ruff check%--output-format=json%"),
		    CommandPattern::Like("ruff check%--output-format json%"),
		    CommandPattern::Like("ruff%--format=json%"),
		    CommandPattern::Like("ruff%--format json%"),
		};
	}
};

} // namespace duckdb
