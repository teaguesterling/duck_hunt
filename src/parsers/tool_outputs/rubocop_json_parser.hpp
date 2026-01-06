#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for RuboCop JSON output.
 * Handles Ruby static analysis and style checking results.
 */
class RuboCopJSONParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "rubocop_json";
	}
	std::string getName() const override {
		return "rubocop_json";
	}
	int getPriority() const override {
		return 70;
	} // Medium-high priority for Ruby linting
	std::string getCategory() const override {
		return "tool_output";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Like("rubocop%-f json%"),
		    CommandPattern::Like("rubocop%--format json%"),
		    CommandPattern::Like("bundle exec rubocop%-f json%"),
		    CommandPattern::Regexp("rubocop.*(-f|--format)[= ]?json"),
		};
	}

private:
	bool isValidRuboCopJSON(const std::string &content) const;
};

} // namespace duckdb
