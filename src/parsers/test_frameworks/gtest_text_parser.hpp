#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Parser for Google Test (gtest) output.
 * Handles test results, failures, and summary information.
 */
class GTestTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "gtest_text";
	}
	std::string getName() const override {
		return "gtest";
	}
	std::string getDescription() const override {
		return "Google Test (gtest) output format";
	}
	int getPriority() const override {
		return 80;
	} // HIGH
	std::string getCategory() const override {
		return "test_framework";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Regexp(".*_test$"), // Common gtest binary naming
		    CommandPattern::Regexp(".*_tests$"),
		    CommandPattern::Like("%--gtest_%"), // gtest flags
		};
	}
};

} // namespace duckdb
