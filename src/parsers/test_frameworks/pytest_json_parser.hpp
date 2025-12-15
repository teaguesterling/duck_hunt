#pragma once

#include "parsers/base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for pytest JSON output.
 * Handles format: {"tests": [{"nodeid": "file.py::test_name", "outcome": "passed", ...}]}
 */
class PytestJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::PYTEST_JSON; }
    std::string getName() const override { return "pytest_json"; }
    int getPriority() const override { return 130; }  // Higher than text pytest parser
    std::string getCategory() const override { return "test_framework_json"; }

private:
    bool isValidPytestJSON(const std::string& content) const;
};

} // namespace duckdb