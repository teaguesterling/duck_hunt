#pragma once

#include "parsers/base/parser_interface.hpp"
#include <unordered_map>

namespace duckdb {

// Forward declaration for internal failure info structure
struct FailureInfo;

/**
 * Parser for pytest text output.
 * Handles format: "file.py::test_name STATUS"
 * Extracts line numbers from FAILURES section for failed tests.
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
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("pytest"),         CommandPattern::Like("pytest %"),
		    CommandPattern::Like("python -m pytest%"), CommandPattern::Like("python3 -m pytest%"),
		    CommandPattern::Regexp("py\\.?test"),
		};
	}

private:
	void parseTestLine(const std::string &line, int64_t &event_id, std::vector<ValidationEvent> &events,
	                   int32_t log_line_num,
	                   const std::unordered_map<std::string, FailureInfo> &failure_info) const;
};

} // namespace duckdb
