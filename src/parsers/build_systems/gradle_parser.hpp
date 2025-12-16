#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Gradle build output.
 * Handles Gradle task execution, compilation errors, test results,
 * Checkstyle violations, SpotBugs findings, and Android Lint issues.
 */
class GradleParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "gradle_build"; }
    std::string getName() const override { return "Gradle Build Parser"; }
    int getPriority() const override { return 80; }
    std::string getCategory() const override { return "build_system"; }
    std::string getDescription() const override { return "Gradle build output"; }
    std::vector<std::string> getAliases() const override { return {"gradle"}; }
};

} // namespace duckdb
