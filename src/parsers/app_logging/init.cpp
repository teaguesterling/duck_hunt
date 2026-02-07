#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "python_logging_parser.hpp"
#include "log4j_parser.hpp"
#include "logrus_parser.hpp"
#include "winston_parser.hpp"
#include "pino_parser.hpp"
#include "bunyan_parser.hpp"
#include "serilog_parser.hpp"
#include "nlog_parser.hpp"
#include "ruby_logger_parser.hpp"
#include "rails_log_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all app logging parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(AppLogging);

void RegisterAppLoggingParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<P<PythonLoggingParser>>(
	    "python_logging", "Python Logging Parser", ParserCategory::APP_LOGGING, "Python standard logging module output",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"python", "logging"}));

	registry.registerParser(make_uniq<P<Log4jParser>>(
	    "log4j", "Log4j Parser", ParserCategory::APP_LOGGING, "Java Log4j/Log4j2 log output", ParserPriority::HIGH,
	    std::vector<std::string> {"log4j2"}, std::vector<std::string> {"java", "logging"}));

	registry.registerParser(make_uniq<P<LogrusParser>>(
	    "logrus", "Logrus Parser", ParserCategory::APP_LOGGING, "Go Logrus structured logging output",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"go", "logging"}));

	registry.registerParser(make_uniq<P<WinstonParser>>(
	    "winston", "Winston Parser", ParserCategory::APP_LOGGING, "Node.js Winston logger output", ParserPriority::HIGH,
	    std::vector<std::string> {}, std::vector<std::string> {"javascript", "logging"}));

	registry.registerParser(make_uniq<P<PinoParser>>(
	    "pino", "Pino Parser", ParserCategory::APP_LOGGING, "Node.js Pino logger output", ParserPriority::HIGH,
	    std::vector<std::string> {}, std::vector<std::string> {"javascript", "logging"}));

	registry.registerParser(make_uniq<P<BunyanParser>>(
	    "bunyan", "Bunyan Parser", ParserCategory::APP_LOGGING, "Node.js Bunyan logger output", ParserPriority::HIGH,
	    std::vector<std::string> {}, std::vector<std::string> {"javascript", "logging"}));

	registry.registerParser(make_uniq<P<SerilogParser>>(
	    "serilog", "Serilog Parser", ParserCategory::APP_LOGGING, ".NET Serilog structured logging output",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"dotnet", "logging"}));

	registry.registerParser(make_uniq<P<NLogParser>>(
	    "nlog", "NLog Parser", ParserCategory::APP_LOGGING, ".NET NLog logger output", ParserPriority::HIGH,
	    std::vector<std::string> {}, std::vector<std::string> {"dotnet", "logging"}));

	registry.registerParser(make_uniq<P<RubyLoggerParser>>(
	    "ruby_logger", "Ruby Logger Parser", ParserCategory::APP_LOGGING, "Ruby standard Logger output",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"ruby", "logging"}));

	registry.registerParser(make_uniq<P<RailsLogParser>>(
	    "rails_log", "Rails Log Parser", ParserCategory::APP_LOGGING, "Ruby on Rails application log output",
	    ParserPriority::HIGH, std::vector<std::string> {"rails"}, std::vector<std::string> {"ruby", "logging", "web"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(AppLogging);

} // namespace duckdb
