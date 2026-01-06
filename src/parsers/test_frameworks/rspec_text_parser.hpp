#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

class RSpecTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "rspec_text";
	}
	std::string getName() const override {
		return "rspec";
	}
	std::string getDescription() const override {
		return "Ruby RSpec test output format";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "test_framework";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("rspec"),
		    CommandPattern::Like("rspec %"),
		    CommandPattern::Like("bundle exec rspec%"),
		    CommandPattern::Regexp("(bundle exec )?rspec"),
		};
	}
};

} // namespace duckdb
