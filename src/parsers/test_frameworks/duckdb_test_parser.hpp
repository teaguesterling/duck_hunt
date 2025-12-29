#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

class DuckDBTestParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "duckdb_test"; }
    std::string getName() const override { return "duckdb_test"; }
    std::string getDescription() const override { return "DuckDB unittest output format"; }
    int getPriority() const override { return 80; }
    std::string getCategory() const override { return "test_framework"; }

    // Static method for backward compatibility with legacy code
    static void ParseDuckDBTestOutput(const std::string& content, std::vector<ValidationEvent>& events);
};

} // namespace duckdb
