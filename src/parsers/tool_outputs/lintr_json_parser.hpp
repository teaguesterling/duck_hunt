#pragma once

#include "parsers/base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for lintr JSON output (R linter).
 * Handles format: [{"filename":"test.R","line_number":1,"column_number":1,"type":"style","message":"...","linter":"..."}]
 */
class LintrJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "lintr_json"; }
    std::string getName() const override { return "lintr"; }
    int getPriority() const override { return 120; }
    std::string getCategory() const override { return "linter_json"; }

private:
    bool isValidLintrJSON(const std::string& content) const;
};

} // namespace duckdb
