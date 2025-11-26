#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "validation_event_types.hpp"
#include "read_workflow_logs_function.hpp"

namespace duckdb {

TableFunction GetParseWorkflowLogsFunction();

} // namespace duckdb