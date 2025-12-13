#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"

namespace duckdb {

// Get the duck_hunt_formats table function
TableFunction GetDuckHuntFormatsFunction();

} // namespace duckdb
