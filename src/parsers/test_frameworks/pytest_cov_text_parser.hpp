#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

class PytestCovTextParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;

    std::string getFormatName() const override { return "pytest_cov_text"; }
    std::string getName() const override { return "pytest_cov"; }
    std::string getDescription() const override { return "Python pytest-cov text output with coverage"; }
    int getPriority() const override { return 80; }
    std::string getCategory() const override { return "test_framework"; }
};

} // namespace duckdb
