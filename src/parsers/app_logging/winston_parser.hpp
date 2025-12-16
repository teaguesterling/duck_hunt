#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class WinstonParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "winston"; }
    std::string getName() const override { return "winston"; }
    int getPriority() const override { return 45; }  // Lower than JSONL to avoid false matches
    std::string getCategory() const override { return "app_logging"; }
};

} // namespace duckdb
