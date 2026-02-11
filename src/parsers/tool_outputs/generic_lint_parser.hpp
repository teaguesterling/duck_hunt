#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

class GenericLintParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "generic_lint";
	}
	std::string getName() const override {
		return "lint";
	}
	std::string getDescription() const override {
		return "Generic linting output format";
	}
	int getPriority() const override {
		return 30;
	} // LOW - fallback parser
	std::string getCategory() const override {
		return "linting_tool";
	}

	// Streaming support (Phase 3)
	bool supportsStreaming() const override {
		return true;
	}
	std::vector<ValidationEvent> parseLine(const std::string &line, int32_t line_number,
	                                       int64_t &event_id) const override;

	// Static method for backward compatibility with legacy code
	static void ParseGenericLint(const std::string &content, std::vector<ValidationEvent> &events);
};

} // namespace duckdb
