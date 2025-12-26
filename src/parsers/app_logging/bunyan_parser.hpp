#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class BunyanParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "bunyan"; }
    std::string getName() const override { return "bunyan"; }
    int getPriority() const override { return 60; }  // Higher than generic JSONL (50) - more specific format
    std::string getCategory() const override { return "app_logging"; }
};

} // namespace duckdb
