#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "aws_cloudtrail_parser.hpp"
#include "gcp_cloud_logging_parser.hpp"
#include "azure_activity_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all cloud log parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(CloudLogs);

void RegisterCloudLogsParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<P<AWSCloudTrailParser>>(
	    "aws_cloudtrail", "AWS CloudTrail Parser", ParserCategory::CLOUD_AUDIT, "AWS CloudTrail audit logs",
	    ParserPriority::HIGH, std::vector<std::string> {"cloudtrail"}, std::vector<std::string> {"cloud", "security"}));

	registry.registerParser(make_uniq<P<GCPCloudLoggingParser>>(
	    "gcp_cloud_logging", "GCP Cloud Logging Parser", ParserCategory::CLOUD_AUDIT,
	    "Google Cloud Logging (Stackdriver) logs", ParserPriority::HIGH,
	    std::vector<std::string> {"stackdriver", "gcp_logging"}, std::vector<std::string> {"cloud"}));

	registry.registerParser(make_uniq<P<AzureActivityParser>>(
	    "azure_activity", "Azure Activity Parser", ParserCategory::CLOUD_AUDIT, "Azure Activity/Audit logs",
	    ParserPriority::HIGH, std::vector<std::string> {"azure"}, std::vector<std::string> {"cloud"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(CloudLogs);

} // namespace duckdb
