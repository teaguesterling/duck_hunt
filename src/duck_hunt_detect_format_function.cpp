#include "include/duck_hunt_detect_format_function.hpp"
#include "include/read_duck_hunt_log_function.hpp"
#include "core/parser_registry.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"

namespace duckdb {

/**
 * Scalar function: duck_hunt_detect_format(content VARCHAR) -> VARCHAR
 *
 * Detects the format of log/test output content using the same logic
 * as read_duck_hunt_log(..., 'auto').
 *
 * Returns the format name that would be used for parsing, or 'unknown'
 * if no parser can handle the content.
 */
static void DuckHuntDetectFormatFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &content_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<string_t, string_t>(content_vector, result, count, [&](string_t content_str) {
		std::string content = content_str.GetString();

		if (content.empty()) {
			return StringVector::AddString(result, "unknown");
		}

		// Use modular parser registry for auto-detection (priority-ordered)
		auto &registry = ParserRegistry::getInstance();
		IParser *parser = registry.findParser(content);

		if (parser) {
			return StringVector::AddString(result, parser->getFormatName());
		}

		// No format detected
		return StringVector::AddString(result, "unknown");
	});
}

// Deliberately NOT marked SetFallible(). Audited transitively: this reaches
// ParserRegistry::findParser -> IParser::canParse for every registered parser.
// No canParse() implementation throws, directly or through a helper — every
// throw under src/parsers lives in parse()/parseWithContext(), which is the
// table function path, not this one. The config-based parsers' detection regex
// goes through SafeParsing::SafeRegexSearch, which catches and returns false
// rather than propagating. Empty input and "no parser matched" both return
// 'unknown'. Marking is optimizer-visible on the pinned DuckDB (`errors` feeds
// Expression::CanThrow(), which gates conjunct reordering, filter pushdown and
// dictionary caching), so it is left unset rather than set defensively.
ScalarFunction GetDuckHuntDetectFormatFunction() {
	return ScalarFunction("duck_hunt_detect_format", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
	                      DuckHuntDetectFormatFunction);
}

} // namespace duckdb
