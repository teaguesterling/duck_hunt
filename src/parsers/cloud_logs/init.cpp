#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "aws_cloudtrail_parser.hpp"
#include "gcp_cloud_logging_parser.hpp"
#include "azure_activity_parser.hpp"


namespace duckdb {

/**
 * AWS CloudTrail parser wrapper.
 */
class AWSCloudTrailParserImpl : public BaseParser {
public:
    AWSCloudTrailParserImpl()
        : BaseParser("aws_cloudtrail",
                     "AWS CloudTrail Parser",
                     ParserCategory::CLOUD_AUDIT,
                     "AWS CloudTrail audit logs",
                     ParserPriority::HIGH) {
        addAlias("cloudtrail");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    AWSCloudTrailParser parser_;
};

/**
 * GCP Cloud Logging parser wrapper.
 */
class GCPCloudLoggingParserImpl : public BaseParser {
public:
    GCPCloudLoggingParserImpl()
        : BaseParser("gcp_cloud_logging",
                     "GCP Cloud Logging Parser",
                     ParserCategory::CLOUD_AUDIT,
                     "Google Cloud Logging (Stackdriver) logs",
                     ParserPriority::HIGH) {
        addAlias("stackdriver");
        addAlias("gcp_logging");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    GCPCloudLoggingParser parser_;
};

/**
 * Azure Activity parser wrapper.
 */
class AzureActivityParserImpl : public BaseParser {
public:
    AzureActivityParserImpl()
        : BaseParser("azure_activity",
                     "Azure Activity Parser",
                     ParserCategory::CLOUD_AUDIT,
                     "Azure Activity/Audit logs",
                     ParserPriority::HIGH) {
        addAlias("azure");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    AzureActivityParser parser_;
};

/**
 * Register all cloud log parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(CloudLogs);

void RegisterCloudLogsParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<AWSCloudTrailParserImpl>());
    registry.registerParser(make_uniq<GCPCloudLoggingParserImpl>());
    registry.registerParser(make_uniq<AzureActivityParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(CloudLogs);


} // namespace duckdb
