#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

class JUnitTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "junit_text";
	}
	std::string getName() const override {
		return "junit";
	}
	std::string getDescription() const override {
		return "JUnit/Maven test output in text format";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "test_framework";
	}
};

} // namespace duckdb
