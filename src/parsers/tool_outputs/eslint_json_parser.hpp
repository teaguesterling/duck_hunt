#pragma once

#include "../base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for ESLint JSON output.
 * Handles format: [{"filePath":"/test.js","messages":[{"ruleId":"no-unused-vars",...}]}]
 */
class ESLintJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::ESLINT_JSON; }
    std::string getName() const override { return "eslint"; }
    int getPriority() const override { return 120; }  // Higher than text parsers
    std::string getCategory() const override { return "linter_json"; }

private:
    bool isValidESLintJSON(const std::string& content) const;
};

} // namespace duckdb