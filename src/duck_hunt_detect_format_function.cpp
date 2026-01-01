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

		// Step 1: Try legacy format detection (same order as read_duck_hunt_log)
		TestResultFormat format = DetectTestResultFormat(content);

		if (format != TestResultFormat::UNKNOWN && format != TestResultFormat::AUTO) {
			// Legacy detection found a format
			std::string format_name = TestResultFormatToString(format);
			return StringVector::AddString(result, format_name);
		}

		// Step 2: Try new modular parser registry auto-detection
		auto &registry = ParserRegistry::getInstance();
		IParser *parser = registry.findParser(content);

		if (parser) {
			return StringVector::AddString(result, parser->getFormatName());
		}

		// No format detected
		return StringVector::AddString(result, "unknown");
	});
}

ScalarFunction GetDuckHuntDetectFormatFunction() {
	return ScalarFunction("duck_hunt_detect_format", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
	                      DuckHuntDetectFormatFunction);
}

} // namespace duckdb
