#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Maven build output.
 * Handles Maven compilation errors, JUnit test failures, Checkstyle violations,
 * SpotBugs findings, PMD violations, and dependency analysis.
 */
class MavenParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "maven_build"; }
    std::string getName() const override { return "Maven Build Parser"; }
    int getPriority() const override { return 80; }
    std::string getCategory() const override { return "build_system"; }
    std::string getDescription() const override { return "Apache Maven build output"; }
    std::vector<std::string> getAliases() const override { return {"maven", "mvn"}; }
};

} // namespace duckdb
