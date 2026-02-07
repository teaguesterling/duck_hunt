#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "jsonl_parser.hpp"
#include "logfmt_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all structured log parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(StructuredLogs);

void RegisterStructuredLogsParsers(ParserRegistry &registry) {
	// Generic formats - lower priority than specific JSON/logfmt loggers
	registry.registerParser(make_uniq<P<JSONLParser>>("jsonl", "JSONL Parser", ParserCategory::STRUCTURED_LOG,
	                                                  "JSON Lines (JSONL/NDJSON) log format", ParserPriority::MEDIUM,
	                                                  std::vector<std::string> {"ndjson", "json_lines"},
	                                                  std::vector<std::string> {"logging"}));

	registry.registerParser(make_uniq<P<LogfmtParser>>(
	    "logfmt", "Logfmt Parser", ParserCategory::STRUCTURED_LOG, "Logfmt key=value log format",
	    ParserPriority::MEDIUM, std::vector<std::string> {}, std::vector<std::string> {"logging"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(StructuredLogs);

} // namespace duckdb
