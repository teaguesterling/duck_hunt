#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for Cargo test JSON output (JSONL format).
 * Handles Rust test framework results in streaming JSON format.
 */
class CargoTestJSONParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::CARGO_TEST_JSON; }
    std::string getName() const override { return "cargo_test_json"; }
    int getPriority() const override { return 75; }  // Medium-high priority for Rust testing
    std::string getCategory() const override { return "tool_output"; }

private:
    bool isValidCargoTestJSON(const std::string& content) const;
};

} // namespace duckdb