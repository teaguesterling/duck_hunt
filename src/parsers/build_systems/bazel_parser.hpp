#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Bazel build and test output.
 * Handles INFO, WARNING, ERROR messages, test results, and build summaries.
 */
class BazelParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "bazel_build"; }
    std::string getName() const override { return "bazel"; }
    int getPriority() const override { return 100; }
    std::string getCategory() const override { return "build_system"; }
};

} // namespace duckdb
