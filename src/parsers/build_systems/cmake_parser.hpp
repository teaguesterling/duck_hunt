#pragma once

#include "../base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for CMake build system output.
 * Handles CMake configuration errors, compilation errors, linker errors, and build failures.
 */
class CMakeParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::CMAKE_BUILD; }
    std::string getName() const override { return "cmake"; }
    int getPriority() const override { return 85; }  // High priority for build systems
    std::string getCategory() const override { return "build_system"; }

private:
    bool isValidCMakeOutput(const std::string& content) const;
};

} // namespace duckdb