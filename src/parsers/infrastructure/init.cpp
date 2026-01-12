#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "iptables_parser.hpp"
#include "pf_parser.hpp"
#include "cisco_asa_parser.hpp"
#include "vpc_flow_parser.hpp"
#include "kubernetes_parser.hpp"
#include "windows_event_parser.hpp"
#include "auditd_parser.hpp"
#include "s3_access_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all infrastructure parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(Infrastructure);

void RegisterInfrastructureParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<P<IptablesParser>>(
	    "iptables", "Iptables Parser", ParserCategory::INFRASTRUCTURE, "Linux iptables firewall log",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"infrastructure", "security"}));

	registry.registerParser(make_uniq<P<PfParser>>(
	    "pf", "PF Parser", ParserCategory::INFRASTRUCTURE, "BSD PF (Packet Filter) firewall log", ParserPriority::HIGH,
	    std::vector<std::string> {"pf_firewall"}, std::vector<std::string> {"infrastructure", "security"}));

	registry.registerParser(make_uniq<P<CiscoAsaParser>>(
	    "cisco_asa", "Cisco ASA Parser", ParserCategory::INFRASTRUCTURE, "Cisco ASA firewall log", ParserPriority::HIGH,
	    std::vector<std::string> {"asa"}, std::vector<std::string> {"infrastructure", "security"}));

	registry.registerParser(make_uniq<P<VpcFlowParser>>(
	    "vpc_flow", "VPC Flow Parser", ParserCategory::INFRASTRUCTURE, "AWS/GCP VPC flow log", ParserPriority::HIGH,
	    std::vector<std::string> {"vpc_flow_log"}, std::vector<std::string> {"infrastructure", "cloud"}));

	registry.registerParser(make_uniq<P<KubernetesParser>>(
	    "kubernetes", "Kubernetes Parser", ParserCategory::INFRASTRUCTURE, "Kubernetes container/pod log",
	    ParserPriority::HIGH, std::vector<std::string> {"k8s"}, std::vector<std::string> {"infrastructure", "cloud"}));

	registry.registerParser(make_uniq<P<WindowsEventParser>>(
	    "windows_event", "Windows Event Parser", ParserCategory::INFRASTRUCTURE, "Windows Event Log",
	    ParserPriority::HIGH, std::vector<std::string> {"windows", "eventlog"},
	    std::vector<std::string> {"infrastructure", "security"}));

	registry.registerParser(make_uniq<P<AuditdParser>>(
	    "auditd", "Auditd Parser", ParserCategory::INFRASTRUCTURE, "Linux auditd audit log", ParserPriority::HIGH,
	    std::vector<std::string> {"audit"}, std::vector<std::string> {"infrastructure", "security"}));

	registry.registerParser(make_uniq<P<S3AccessParser>>(
	    "s3_access", "S3 Access Parser", ParserCategory::INFRASTRUCTURE, "AWS S3 bucket access log",
	    ParserPriority::HIGH, std::vector<std::string> {"s3_access_log"},
	    std::vector<std::string> {"infrastructure", "cloud"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(Infrastructure);

} // namespace duckdb
