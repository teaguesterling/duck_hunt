#pragma once

#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

/**
 * Get the duck_hunt_load_parser_config scalar function.
 * duck_hunt_load_parser_config(json_config VARCHAR) -> VARCHAR
 * Returns the parser name on success.
 */
ScalarFunction GetDuckHuntLoadParserConfigFunction();

/**
 * Get the duck_hunt_unload_parser scalar function.
 * duck_hunt_unload_parser(format_name VARCHAR) -> BOOLEAN
 * Returns true if parser was unloaded, false if not found or built-in.
 */
ScalarFunction GetDuckHuntUnloadParserFunction();

} // namespace duckdb
