#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class Log4jParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::LOG4J; }
    std::string getName() const override { return "log4j"; }
    int getPriority() const override { return 50; }  // Medium priority for app logs
    std::string getCategory() const override { return "app_logging"; }
};

} // namespace duckdb
