#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "syslog_parser.hpp"
#include "apache_access_parser.hpp"
#include "nginx_access_parser.hpp"

namespace duckdb {

/**
 * Syslog parser wrapper.
 */
class SyslogParserImpl : public BaseParser {
public:
	SyslogParserImpl()
	    : BaseParser("syslog", "Syslog Parser", ParserCategory::SYSTEM_LOG, "Unix/Linux syslog format",
	                 ParserPriority::HIGH) {
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

private:
	SyslogParser parser_;
};

/**
 * Apache access log parser wrapper.
 */
class ApacheAccessParserImpl : public BaseParser {
public:
	ApacheAccessParserImpl()
	    : BaseParser("apache_access", "Apache Access Parser", ParserCategory::WEB_ACCESS,
	                 "Apache HTTP Server access log", ParserPriority::HIGH) {
		addAlias("apache");
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

private:
	ApacheAccessParser parser_;
};

/**
 * Nginx access log parser wrapper.
 */
class NginxAccessParserImpl : public BaseParser {
public:
	NginxAccessParserImpl()
	    : BaseParser("nginx_access", "Nginx Access Parser", ParserCategory::WEB_ACCESS, "Nginx HTTP Server access log",
	                 ParserPriority::HIGH) {
		addAlias("nginx");
	}

	bool canParse(const std::string &content) const override {
		return parser_.canParse(content);
	}

	std::vector<ValidationEvent> parse(const std::string &content) const override {
		return parser_.parse(content);
	}

private:
	NginxAccessParser parser_;
};

/**
 * Register all web access parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(WebAccess);

void RegisterWebAccessParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<SyslogParserImpl>());
	registry.registerParser(make_uniq<ApacheAccessParserImpl>());
	registry.registerParser(make_uniq<NginxAccessParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(WebAccess);

} // namespace duckdb
