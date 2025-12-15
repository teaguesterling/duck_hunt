#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class S3AccessParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::S3_ACCESS; }
    std::string getName() const override { return "s3_access"; }
    int getPriority() const override { return 50; }
    std::string getCategory() const override { return "infrastructure"; }
};

} // namespace duckdb
