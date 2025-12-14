#pragma once

#include "../base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for SQLFluff JSON output (SQL linter).
 * Handles format: [{"filepath":"query.sql","violations":[{"line_no":1,"line_pos":1,"code":"L001","description":"..."}]}]
 */
class SqlfluffJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::SQLFLUFF_JSON; }
    std::string getName() const override { return "sqlfluff"; }
    int getPriority() const override { return 120; }
    std::string getCategory() const override { return "linter_json"; }

private:
    bool isValidSqlfluffJSON(const std::string& content) const;
};

} // namespace duckdb
