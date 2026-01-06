#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Black Python code formatter output.
 * Handles formatting suggestions, summaries, and diff output.
 */
class BlackParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "black_text";
	}
	std::string getName() const override {
		return "black";
	}
	int getPriority() const override {
		return 75;
	} // Medium-high priority for formatting
	std::string getCategory() const override {
		return "linting_tool";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Like("black --check%"),
		    CommandPattern::Like("black --diff%"),
		    CommandPattern::Like("python -m black%"),
		    CommandPattern::Regexp("black\\s+(--check|--diff)"),
		};
	}

private:
	bool isValidBlackOutput(const std::string &content) const;
};

} // namespace duckdb
