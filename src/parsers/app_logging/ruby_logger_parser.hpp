#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

class RubyLoggerParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::RUBY_LOGGER; }
    std::string getName() const override { return "ruby_logger"; }
    int getPriority() const override { return 50; }
    std::string getCategory() const override { return "app_logging"; }
};

} // namespace duckdb
