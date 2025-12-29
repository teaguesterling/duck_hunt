#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

class GenericLintParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "generic_lint"; }
    std::string getName() const override { return "lint"; }
    std::string getDescription() const override { return "Generic linting output format"; }
    int getPriority() const override { return 30; }  // LOW - fallback parser
    std::string getCategory() const override { return "linting_tool"; }

    // Static method for backward compatibility with legacy code
    static void ParseGenericLint(const std::string& content, std::vector<ValidationEvent>& events);
};

} // namespace duckdb
