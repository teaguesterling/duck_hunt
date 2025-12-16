#pragma once

#include "parsers/base/parser_interface.hpp"
#include "yyjson.hpp"
#include <map>

namespace duckdb {

/**
 * Parser for Go test JSON output.
 * Handles format: {"Action":"run","Package":"pkg","Test":"TestName","Elapsed":0.1}
 * Multiple lines per test (run -> pass/fail/skip)
 */
class GoTestJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "gotest_json"; }
    std::string getName() const override { return "go_test"; }
    int getPriority() const override { return 120; }  // Higher than text parsers
    std::string getCategory() const override { return "test_framework_json"; }

private:
    bool isValidGoTestJSON(const std::string& content) const;
};

} // namespace duckdb