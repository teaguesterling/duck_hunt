#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Python pip/setuptools build output.
 * Handles pip wheel building errors, C extension compilation, pytest results, and setuptools failures.
 */
class PythonBuildParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "python_build"; }
    std::string getName() const override { return "Python Build Parser"; }
    int getPriority() const override { return 80; }
    std::string getCategory() const override { return "build_system"; }
    std::string getDescription() const override { return "Python pip/setuptools build output"; }
    std::vector<std::string> getAliases() const override { return {"pip", "setuptools"}; }
};

} // namespace duckdb
