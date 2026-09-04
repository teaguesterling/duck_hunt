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

// Both of these raise InvalidInputException from inside their execute callback,
// so they must declare themselves fallible. DuckDB v1.5 already has this flag
// (BaseScalarFunction::SetFallible, function.hpp:211) and already consults it
// through Expression::CanThrow(); v2.0 additionally *enforces* it and turns an
// undeclared throw into:
//
//   INTERNAL Error: Scalar function "duck_hunt_load_parser_config" threw an
//   execution error, but the function is not marked as fallible - the function
//   must call SetFallible().
//
// Marking is not free — on the pin `errors` gates conjunct reordering, filter
// pushdown and dictionary-expression caching — so only functions with a real
// escaping throw are marked. The ScalarFunction is hoisted to a local because
// SetFallible() needs an object to act on.
ScalarFunction GetDuckHuntLoadParserConfigFunction() {
	// Throws InvalidInputException for an invalid JSON config and for attempts
	// to replace a built-in parser (the inner catch rethrows, it does not
	// swallow).
	ScalarFunction fun("duck_hunt_load_parser_config", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
	                   DuckHuntLoadParserConfigFunction);
	fun.SetFallible();
	return fun;
}

ScalarFunction GetDuckHuntUnloadParserFunction() {
	// Throws InvalidInputException when asked to unload a built-in parser.
	ScalarFunction fun("duck_hunt_unload_parser", {LogicalType::VARCHAR}, LogicalType::BOOLEAN,
	                   DuckHuntUnloadParserFunction);
	fun.SetFallible();
	return fun;
}

} // namespace duckdb
