#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Go test text output (default format, without -json flag).
 * Handles test results from `go test` command.
 *
 * Example format:
 * === RUN   TestAdd
 * --- PASS: TestAdd (0.00s)
 * === RUN   TestSubtract
 *     main_test.go:15: Expected 5 but got 4
 * --- FAIL: TestSubtract (0.00s)
 * FAIL
 * exit status 1
 * FAIL    example.com/myapp    0.001s
 */
class GoTestTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "gotest_text";
	}
	std::string getName() const override {
		return "Go Test Text Parser";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "test_framework";
	}
	std::string getDescription() const override {
		return "Go test text output (default format)";
	}
	std::vector<std::string> getAliases() const override {
		return {"gotest"};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Like("go test%"),
		    CommandPattern::Regexp("go\\s+test\\s+(?!.*-json)"),
		};
	}
	std::vector<std::string> getGroups() const override {
		return {"go", "test"};
	}
};

} // namespace duckdb
