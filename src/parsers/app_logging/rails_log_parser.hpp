#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class RailsLogParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "rails_log"; }
    std::string getName() const override { return "rails_log"; }
    int getPriority() const override { return 50; }
    std::string getCategory() const override { return "app_logging"; }
};

} // namespace duckdb
