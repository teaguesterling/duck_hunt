#pragma once

#include "parsers/base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for pytest JSON output.
 * Handles format: {"tests": [{"nodeid": "file.py::test_name", "outcome": "passed", ...}]}
 */
class PytestJSONParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "pytest_json";
	}
	std::string getName() const override {
		return "pytest_json";
	}
	int getPriority() const override {
		return 130;
	} // Higher than text pytest parser
	std::string getCategory() const override {
		return "test_framework_json";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		// No command patterns - pytest JSON output requires plugins like pytest-json-report
		// and there's no standard command-line flag to request it. Let pytest_text handle
		// command matching; pytest_json will be used via content detection (canParse).
		return {};
	}

private:
	bool isValidPytestJSON(const std::string &content) const;
};

} // namespace duckdb
