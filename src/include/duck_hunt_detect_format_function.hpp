#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

// Get the duck_hunt_detect_format scalar function
ScalarFunction GetDuckHuntDetectFormatFunction();

} // namespace duckdb
