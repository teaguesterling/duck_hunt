#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for ShellCheck text output.
 * Handles linting issues from the ShellCheck shell script linter.
 *
 * Example format:
 * In script.sh line 3:
 * echo $foo
 *      ^--^ SC2086: Double quote to prevent globbing and word splitting.
 */
class ShellcheckTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "shellcheck_text";
	}
	std::string getName() const override {
		return "ShellCheck Text Parser";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "linting_tool";
	}
	std::string getDescription() const override {
		return "ShellCheck shell script linter text output";
	}
	std::vector<std::string> getAliases() const override {
		return {"shellcheck"};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("shellcheck"),
		    CommandPattern::Like("shellcheck %"),
		    CommandPattern::Regexp("shellcheck\\s+(?!.*(-f|--format)[= ]?json)"),
		};
	}
	std::vector<std::string> getGroups() const override {
		return {"shell", "lint"};
	}
};

} // namespace duckdb
