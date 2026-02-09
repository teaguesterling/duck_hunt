#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for autopep8 text output (Python formatter).
 * Handles diff output, error messages, and summary statistics.
 */
class Autopep8TextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "autopep8_text";
	}
	std::string getName() const override {
		return "autopep8";
	}
	int getPriority() const override {
		return 100;
	}
	std::string getCategory() const override {
		return "formatter";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("autopep8"),
		    CommandPattern::Like("autopep8 %"),
		    CommandPattern::Like("autopep8 --diff%"),
		    CommandPattern::Like("python -m autopep8%"),
		};
	}
};

} // namespace duckdb
