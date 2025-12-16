#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Make build system error output.
 * Handles GCC/Clang compilation errors, make failures, and linker errors.
 */
class MakeParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "make_error"; }
    std::string getName() const override { return "make"; }
    int getPriority() const override { return 90; }  // High priority for common build errors
    std::string getCategory() const override { return "build_system"; }

private:
    bool isValidMakeError(const std::string& content) const;
};

} // namespace duckdb