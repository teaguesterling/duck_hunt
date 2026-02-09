#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Bandit (Python security linter) text output.
 * Detects security issues with severity and confidence levels.
 */
class BanditTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "bandit_text";
	}
	std::string getName() const override {
		return "bandit";
	}
	std::string getDescription() const override {
		return "Python Bandit security linter output";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "linting_tool";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("bandit"),
		    CommandPattern::Like("bandit %"),
		    CommandPattern::Like("python -m bandit%"),
		};
	}
};

} // namespace duckdb
