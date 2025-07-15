#pragma once

#include "../base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for MyPy type checker output.
 * Handles error/warning messages, summaries, and success cases.
 */
class MypyParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::MYPY_TEXT; }
    std::string getName() const override { return "mypy"; }
    int getPriority() const override { return 85; }  // High priority for Python type checking
    std::string getCategory() const override { return "linting_tool"; }

private:
    bool isValidMypyOutput(const std::string& content) const;
};

} // namespace duckdb