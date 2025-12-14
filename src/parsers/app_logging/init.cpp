#include "../../core/new_parser_registry.hpp"
#include "../base/base_parser.hpp"
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
namespace log_parsers {

/**
 * Python logging parser wrapper.
 */
class PythonLoggingParserImpl : public BaseParser {
public:
    PythonLoggingParserImpl()
        : BaseParser("python_logging",
                     "Python Logging Parser",
                     ParserCategory::APP_LOGGING,
                     "Python standard logging module output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    PythonLoggingParser parser_;
};

/**
 * Log4j parser wrapper.
 */
class Log4jParserImpl : public BaseParser {
public:
    Log4jParserImpl()
        : BaseParser("log4j",
                     "Log4j Parser",
                     ParserCategory::APP_LOGGING,
                     "Java Log4j/Log4j2 log output",
                     ParserPriority::HIGH) {
        addAlias("log4j2");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    Log4jParser parser_;
};

/**
 * Logrus parser wrapper.
 */
class LogrusParserImpl : public BaseParser {
public:
    LogrusParserImpl()
        : BaseParser("logrus",
                     "Logrus Parser",
                     ParserCategory::APP_LOGGING,
                     "Go Logrus structured logging output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    LogrusParser parser_;
};

/**
 * Winston parser wrapper.
 */
class WinstonParserImpl : public BaseParser {
public:
    WinstonParserImpl()
        : BaseParser("winston",
                     "Winston Parser",
                     ParserCategory::APP_LOGGING,
                     "Node.js Winston logger output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    WinstonParser parser_;
};

/**
 * Pino parser wrapper.
 */
class PinoParserImpl : public BaseParser {
public:
    PinoParserImpl()
        : BaseParser("pino",
                     "Pino Parser",
                     ParserCategory::APP_LOGGING,
                     "Node.js Pino logger output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    PinoParser parser_;
};

/**
 * Bunyan parser wrapper.
 */
class BunyanParserImpl : public BaseParser {
public:
    BunyanParserImpl()
        : BaseParser("bunyan",
                     "Bunyan Parser",
                     ParserCategory::APP_LOGGING,
                     "Node.js Bunyan logger output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    BunyanParser parser_;
};

/**
 * Serilog parser wrapper.
 */
class SerilogParserImpl : public BaseParser {
public:
    SerilogParserImpl()
        : BaseParser("serilog",
                     "Serilog Parser",
                     ParserCategory::APP_LOGGING,
                     ".NET Serilog structured logging output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    SerilogParser parser_;
};

/**
 * NLog parser wrapper.
 */
class NLogParserImpl : public BaseParser {
public:
    NLogParserImpl()
        : BaseParser("nlog",
                     "NLog Parser",
                     ParserCategory::APP_LOGGING,
                     ".NET NLog logger output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    NLogParser parser_;
};

/**
 * Ruby Logger parser wrapper.
 */
class RubyLoggerParserImpl : public BaseParser {
public:
    RubyLoggerParserImpl()
        : BaseParser("ruby_logger",
                     "Ruby Logger Parser",
                     ParserCategory::APP_LOGGING,
                     "Ruby standard Logger output",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    RubyLoggerParser parser_;
};

/**
 * Rails Log parser wrapper.
 */
class RailsLogParserImpl : public BaseParser {
public:
    RailsLogParserImpl()
        : BaseParser("rails_log",
                     "Rails Log Parser",
                     ParserCategory::APP_LOGGING,
                     "Ruby on Rails application log output",
                     ParserPriority::HIGH) {
        addAlias("rails");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    RailsLogParser parser_;
};

/**
 * Register all app logging parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(AppLogging);

void RegisterAppLoggingParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<PythonLoggingParserImpl>());
    registry.registerParser(make_uniq<Log4jParserImpl>());
    registry.registerParser(make_uniq<LogrusParserImpl>());
    registry.registerParser(make_uniq<WinstonParserImpl>());
    registry.registerParser(make_uniq<PinoParserImpl>());
    registry.registerParser(make_uniq<BunyanParserImpl>());
    registry.registerParser(make_uniq<SerilogParserImpl>());
    registry.registerParser(make_uniq<NLogParserImpl>());
    registry.registerParser(make_uniq<RubyLoggerParserImpl>());
    registry.registerParser(make_uniq<RailsLogParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(AppLogging);

} // namespace log_parsers
} // namespace duckdb
