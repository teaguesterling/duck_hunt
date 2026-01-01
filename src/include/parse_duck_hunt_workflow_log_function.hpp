#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "validation_event_types.hpp"
#include "read_duck_hunt_workflow_log_function.hpp"

namespace duckdb {

TableFunctionSet GetParseDuckHuntWorkflowLogFunction();

} // namespace duckdb
