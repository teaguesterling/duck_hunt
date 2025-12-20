#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"

namespace duckdb {

// Get the duck_hunt_diagnose_parse table function set (for string content)
// Supports: duck_hunt_diagnose_parse(content) and duck_hunt_diagnose_parse(content, emit)
// emit can be 'all', 'valid', or 'invalid'
TableFunctionSet GetDuckHuntDiagnoseParseFunction();

// Get the duck_hunt_diagnose_read table function set (for file paths)
// Supports: duck_hunt_diagnose_read(path) and duck_hunt_diagnose_read(path, emit)
// emit can be 'all', 'valid', or 'invalid'
TableFunctionSet GetDuckHuntDiagnoseReadFunction();

} // namespace duckdb
