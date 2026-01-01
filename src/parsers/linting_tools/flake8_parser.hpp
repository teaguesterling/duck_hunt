#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Flake8 Python linter output.
 * Handles PEP 8 style violations and pyflakes errors.
 */
class Flake8Parser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "flake8_text";
	}
	std::string getName() const override {
		return "flake8";
	}
	int getPriority() const override {
		return 80;
	} // High priority for Python linting
	std::string getCategory() const override {
		return "linting_tool";
	}

private:
	bool isValidFlake8Output(const std::string &content) const;
};

} // namespace duckdb
