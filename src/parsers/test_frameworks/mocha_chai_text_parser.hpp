#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Parser for Mocha/Chai JavaScript test output.
 * Handles test results, failures, and summary information.
 */
class MochaChaiTextParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "mocha_chai_text"; }
    std::string getName() const override { return "mocha"; }
    std::string getDescription() const override { return "Mocha/Chai JavaScript test output"; }
    int getPriority() const override { return 80; }  // HIGH
    std::string getCategory() const override { return "test_framework"; }
};

} // namespace duckdb
