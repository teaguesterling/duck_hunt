#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

class AnsibleTextParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::ANSIBLE_TEXT; }
    std::string getName() const override { return "ansible"; }
    int getPriority() const override { return 85; }
    std::string getCategory() const override { return "infrastructure_tools"; }
};

} // namespace duckdb