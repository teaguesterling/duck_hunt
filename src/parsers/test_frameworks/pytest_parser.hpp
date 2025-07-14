#pragma once

#include "../base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for pytest text output.
 * Handles format: "file.py::test_name STATUS"
 */
class PytestParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::PYTEST_TEXT; }
    std::string getName() const override { return "pytest"; }
    int getPriority() const override { return 100; }  // High priority
    std::string getCategory() const override { return "test_framework"; }

private:
    void parseTestLine(const std::string& line, int64_t& event_id, 
                      std::vector<ValidationEvent>& events) const;
};

} // namespace duckdb