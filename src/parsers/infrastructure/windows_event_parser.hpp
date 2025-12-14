#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

class WindowsEventParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::WINDOWS_EVENT; }
    std::string getName() const override { return "windows_event"; }
    int getPriority() const override { return 50; }
    std::string getCategory() const override { return "infrastructure"; }
};

} // namespace duckdb
