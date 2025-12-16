#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for CMake build output.
 * Handles CMake configuration errors/warnings, GCC/Clang compile errors,
 * linker errors, and make/gmake failures.
 */
class CMakeParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "cmake_build"; }
    std::string getName() const override { return "CMake Build Parser"; }
    int getPriority() const override { return 80; }
    std::string getCategory() const override { return "build_system"; }
    std::string getDescription() const override { return "CMake configuration and build output"; }
    std::vector<std::string> getAliases() const override { return {"cmake"}; }
};

} // namespace duckdb
