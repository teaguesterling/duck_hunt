#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for pytest text output.
 * Handles format: "file.py::test_name STATUS"
 */
class PytestParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "pytest_text";
	}
	std::string getName() const override {
		return "pytest";
	}
	int getPriority() const override {
		return 100;
	} // High priority
	std::string getCategory() const override {
		return "test_framework";
	}

private:
	void parseTestLine(const std::string &line, int64_t &event_id, std::vector<ValidationEvent> &events,
	                   int32_t log_line_num) const;
};

} // namespace duckdb
