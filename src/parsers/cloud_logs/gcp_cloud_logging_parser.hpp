#pragma once

#include "../base/parser_interface.hpp"

namespace duckdb {

class GCPCloudLoggingParser : public IParser {
public:
    bool canParse(const std::string& content) const override;
    std::vector<ValidationEvent> parse(const std::string& content) const override;
    TestResultFormat getFormat() const override { return TestResultFormat::GCP_CLOUD_LOGGING; }
    std::string getName() const override { return "gcp_cloud_logging"; }
    int getPriority() const override { return 54; }  // Medium priority for cloud logs
    std::string getCategory() const override { return "cloud_audit"; }
};

} // namespace duckdb
