#pragma once

#include "parsers/base/parser_interface.hpp"
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
    std::string getFormatName() const override { return "mypy_text"; }
    std::string getName() const override { return "mypy"; }
    int getPriority() const override { return 80; }  // Lower priority than clang-tidy to avoid conflicts
    std::string getCategory() const override { return "linting_tool"; }

private:
    bool isValidMypyOutput(const std::string& content) const;
};

} // namespace duckdb