#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for RuboCop text output.
 * Handles linting issues from the RuboCop Ruby linter.
 *
 * Example format:
 * app/models/user.rb:10:3: C: Style/StringLiterals: Prefer single-quoted strings...
 * app/models/user.rb:15:1: W: Layout/TrailingWhitespace: Trailing whitespace detected.
 */
class RubocopTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "rubocop_text";
	}
	std::string getName() const override {
		return "RuboCop Text Parser";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "linting_tool";
	}
	std::string getDescription() const override {
		return "RuboCop Ruby linter text output";
	}
	std::vector<std::string> getAliases() const override {
		return {};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("rubocop"),
		    CommandPattern::Like("rubocop %"),
		    CommandPattern::Like("bundle exec rubocop %"),
		    CommandPattern::Regexp("rubocop\\s+(?!.*(-f|--format)\\s*json)"),
		};
	}
	std::vector<std::string> getGroups() const override {
		return {"ruby", "lint"};
	}
};

} // namespace duckdb
