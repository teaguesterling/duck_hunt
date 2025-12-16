#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Pylint Python code quality checker output.
 * Handles module headers, messages with severity codes, and ratings.
 */
class PylintParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "pylint_text"; }
    std::string getName() const override { return "pylint"; }
    int getPriority() const override { return 80; }  // High priority for Python quality checking
    std::string getCategory() const override { return "linting_tool"; }

private:
    bool isValidPylintOutput(const std::string& content) const;
};

} // namespace duckdb