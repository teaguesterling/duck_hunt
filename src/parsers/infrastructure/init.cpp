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

/**
 * Iptables parser wrapper.
 */
class IptablesParserImpl : public BaseParser {
public:
    IptablesParserImpl()
        : BaseParser("iptables",
                     "Iptables Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "Linux iptables firewall log",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    IptablesParser parser_;
};

/**
 * PF firewall parser wrapper.
 */
class PfParserImpl : public BaseParser {
public:
    PfParserImpl()
        : BaseParser("pf",
                     "PF Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "BSD PF (Packet Filter) firewall log",
                     ParserPriority::HIGH) {
        addAlias("pf_firewall");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    PfParser parser_;
};

/**
 * Cisco ASA parser wrapper.
 */
class CiscoAsaParserImpl : public BaseParser {
public:
    CiscoAsaParserImpl()
        : BaseParser("cisco_asa",
                     "Cisco ASA Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "Cisco ASA firewall log",
                     ParserPriority::HIGH) {
        addAlias("asa");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    CiscoAsaParser parser_;
};

/**
 * VPC Flow log parser wrapper.
 */
class VpcFlowParserImpl : public BaseParser {
public:
    VpcFlowParserImpl()
        : BaseParser("vpc_flow",
                     "VPC Flow Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "AWS/GCP VPC flow log",
                     ParserPriority::HIGH) {
        addAlias("vpc_flow_log");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    VpcFlowParser parser_;
};

/**
 * Kubernetes parser wrapper.
 */
class KubernetesParserImpl : public BaseParser {
public:
    KubernetesParserImpl()
        : BaseParser("kubernetes",
                     "Kubernetes Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "Kubernetes container/pod log",
                     ParserPriority::HIGH) {
        addAlias("k8s");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    KubernetesParser parser_;
};

/**
 * Windows Event parser wrapper.
 */
class WindowsEventParserImpl : public BaseParser {
public:
    WindowsEventParserImpl()
        : BaseParser("windows_event",
                     "Windows Event Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "Windows Event Log",
                     ParserPriority::HIGH) {
        addAlias("windows");
        addAlias("eventlog");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    WindowsEventParser parser_;
};

/**
 * Auditd parser wrapper.
 */
class AuditdParserImpl : public BaseParser {
public:
    AuditdParserImpl()
        : BaseParser("auditd",
                     "Auditd Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "Linux auditd audit log",
                     ParserPriority::HIGH) {
        addAlias("audit");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    AuditdParser parser_;
};

/**
 * S3 Access parser wrapper.
 */
class S3AccessParserImpl : public BaseParser {
public:
    S3AccessParserImpl()
        : BaseParser("s3_access",
                     "S3 Access Parser",
                     ParserCategory::INFRASTRUCTURE,
                     "AWS S3 bucket access log",
                     ParserPriority::HIGH) {
        addAlias("s3_access_log");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    S3AccessParser parser_;
};

/**
 * Register all infrastructure parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(Infrastructure);

void RegisterInfrastructureParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<IptablesParserImpl>());
    registry.registerParser(make_uniq<PfParserImpl>());
    registry.registerParser(make_uniq<CiscoAsaParserImpl>());
    registry.registerParser(make_uniq<VpcFlowParserImpl>());
    registry.registerParser(make_uniq<KubernetesParserImpl>());
    registry.registerParser(make_uniq<WindowsEventParserImpl>());
    registry.registerParser(make_uniq<AuditdParserImpl>());
    registry.registerParser(make_uniq<S3AccessParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(Infrastructure);


} // namespace duckdb
