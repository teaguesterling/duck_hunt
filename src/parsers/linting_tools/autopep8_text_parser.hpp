#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for autopep8 text output (Python formatter).
 * Handles diff output, error messages, and summary statistics.
 */
class Autopep8TextParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::AUTOPEP8_TEXT; }
    std::string getName() const override { return "autopep8"; }
    int getPriority() const override { return 100; }
    std::string getCategory() const override { return "formatter"; }
};

} // namespace duckdb
