#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class PythonLoggingParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "python_logging"; }
    std::string getName() const override { return "python_logging"; }
    int getPriority() const override { return 50; }  // Medium priority for app logs
    std::string getCategory() const override { return "app_logging"; }
};

} // namespace duckdb
