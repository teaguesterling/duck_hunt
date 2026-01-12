#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "syslog_parser.hpp"
#include "apache_access_parser.hpp"
#include "nginx_access_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all web access parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(WebAccess);

void RegisterWebAccessParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<P<SyslogParser>>("syslog", "Syslog Parser", ParserCategory::SYSTEM_LOG,
	                                                   "Unix/Linux syslog format", ParserPriority::HIGH,
	                                                   std::vector<std::string> {},
	                                                   std::vector<std::string> {"web", "logging"}));

	registry.registerParser(make_uniq<P<ApacheAccessParser>>(
	    "apache_access", "Apache Access Parser", ParserCategory::WEB_ACCESS, "Apache HTTP Server access log",
	    ParserPriority::HIGH, std::vector<std::string> {"apache"}, std::vector<std::string> {"web"}));

	registry.registerParser(make_uniq<P<NginxAccessParser>>(
	    "nginx_access", "Nginx Access Parser", ParserCategory::WEB_ACCESS, "Nginx HTTP Server access log",
	    ParserPriority::HIGH, std::vector<std::string> {"nginx"}, std::vector<std::string> {"web"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(WebAccess);

} // namespace duckdb
