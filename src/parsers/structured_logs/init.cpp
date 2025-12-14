#include "../../core/new_parser_registry.hpp"
#include "../base/base_parser.hpp"
#include "jsonl_parser.hpp"
#include "logfmt_parser.hpp"

namespace duckdb {
namespace log_parsers {

/**
 * JSONL (JSON Lines) parser wrapper.
 */
class JSONLParserImpl : public BaseParser {
public:
    JSONLParserImpl()
        : BaseParser("jsonl",
                     "JSONL Parser",
                     ParserCategory::STRUCTURED_LOG,
                     "JSON Lines (JSONL/NDJSON) log format",
                     ParserPriority::HIGH) {
        addAlias("ndjson");
        addAlias("json_lines");
    }

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    JSONLParser parser_;
};

/**
 * Logfmt parser wrapper.
 */
class LogfmtParserImpl : public BaseParser {
public:
    LogfmtParserImpl()
        : BaseParser("logfmt",
                     "Logfmt Parser",
                     ParserCategory::STRUCTURED_LOG,
                     "Logfmt key=value log format",
                     ParserPriority::HIGH) {}

    bool canParse(const std::string& content) const override {
        return parser_.canParse(content);
    }

    std::vector<ValidationEvent> parse(const std::string& content) const override {
        return parser_.parse(content);
    }

private:
    LogfmtParser parser_;
};

/**
 * Register all structured log parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(StructuredLogs);

void RegisterStructuredLogsParsers(ParserRegistry& registry) {
    registry.registerParser(make_uniq<JSONLParserImpl>());
    registry.registerParser(make_uniq<LogfmtParserImpl>());
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(StructuredLogs);

} // namespace log_parsers
} // namespace duckdb
