#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

class CiscoAsaParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::CISCO_ASA; }
    std::string getName() const override { return "cisco_asa"; }
    int getPriority() const override { return 50; }
    std::string getCategory() const override { return "infrastructure"; }
};

} // namespace duckdb
