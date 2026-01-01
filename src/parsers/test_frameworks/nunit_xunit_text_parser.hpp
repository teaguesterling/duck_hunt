#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

class NUnitXUnitTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "nunit_xunit_text";
	}
	std::string getName() const override {
		return "nunit";
	}
	std::string getDescription() const override {
		return ".NET NUnit/xUnit test output";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "test_framework";
	}
};

} // namespace duckdb
