#include "include/config_parser_functions.hpp"
#include "parsers/config_based/config_parser.hpp"
#include "core/parser_registry.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"

namespace duckdb {

// duck_hunt_load_parser_config(json_config VARCHAR) -> VARCHAR
static void DuckHuntLoadParserConfigFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &json_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<string_t, string_t>(json_vector, result, count, [&](string_t json_config) {
		try {
			auto parser = ConfigBasedParser::FromJson(json_config.GetString());
			std::string format_name = parser->getFormatName();

			// Check if parser with this name already exists
			auto &registry = ParserRegistry::getInstance();
			if (registry.hasFormat(format_name)) {
				// If it's a built-in, error. If it's a custom, unregister first.
				if (registry.isBuiltIn(format_name)) {
					throw InvalidInputException("Cannot replace built-in parser: " + format_name);
				}
				// Unregister existing custom parser
				registry.unregisterParser(format_name);
			}

			// Register the new parser
			registry.registerParser(std::move(parser));

			return StringVector::AddString(result, format_name);
		} catch (const std::exception &e) {
			throw InvalidInputException("Failed to load parser config: " + std::string(e.what()));
		}
	});
}

// duck_hunt_unload_parser(format_name VARCHAR) -> BOOLEAN
static void DuckHuntUnloadParserFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<string_t, bool>(name_vector, result, count, [&](string_t format_name) {
		auto &registry = ParserRegistry::getInstance();
		std::string name = format_name.GetString();

		// Check if it's a built-in parser
		if (registry.isBuiltIn(name)) {
			throw InvalidInputException("Cannot unload built-in parser: " + name);
		}

		// Try to unregister
		return registry.unregisterParser(name);
	});
}

ScalarFunction GetDuckHuntLoadParserConfigFunction() {
	return ScalarFunction("duck_hunt_load_parser_config", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
	                      DuckHuntLoadParserConfigFunction);
}

ScalarFunction GetDuckHuntUnloadParserFunction() {
	return ScalarFunction("duck_hunt_unload_parser", {LogicalType::VARCHAR}, LogicalType::BOOLEAN,
	                      DuckHuntUnloadParserFunction);
}

} // namespace duckdb
