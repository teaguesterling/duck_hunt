#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

class AzureActivityParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    std::string getFormatName() const override { return "azure_activity"; }
    std::string getName() const override { return "azure_activity"; }
    int getPriority() const override { return 55; }  // Medium priority for cloud logs
    std::string getCategory() const override { return "cloud_audit"; }
};

} // namespace duckdb
