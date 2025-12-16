#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class RubyLoggerParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "ruby_logger"; }
    std::string getName() const override { return "ruby_logger"; }
    int getPriority() const override { return 50; }
    std::string getCategory() const override { return "app_logging"; }
};

} // namespace duckdb
