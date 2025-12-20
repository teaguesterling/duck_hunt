#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"

namespace duckdb {

// Get the duck_hunt_diagnose_parse table function (for string content)
TableFunction GetDuckHuntDiagnoseParseFunction();

// Get the duck_hunt_diagnose_read table function (for file paths)
TableFunction GetDuckHuntDiagnoseReadFunction();

} // namespace duckdb
