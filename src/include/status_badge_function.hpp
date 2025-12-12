#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

// Get the status_badge scalar function set (both overloads)
ScalarFunctionSet GetStatusBadgeFunction();

} // namespace duckdb
