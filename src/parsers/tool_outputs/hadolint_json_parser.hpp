#pragma once

#include "parsers/base/parser_interface.hpp"
#include "yyjson.hpp"

namespace duckdb {

/**
 * Parser for Hadolint JSON output.
 * Handles format: [{"file":"Dockerfile","line":1,"column":1,"code":"DL3006","message":"...","level":"warning"}]
 */
class HadolintJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "hadolint_json"; }
    std::string getName() const override { return "hadolint"; }
    int getPriority() const override { return 120; }
    std::string getCategory() const override { return "linter_json"; }

private:
    bool isValidHadolintJSON(const std::string& content) const;
};

} // namespace duckdb
