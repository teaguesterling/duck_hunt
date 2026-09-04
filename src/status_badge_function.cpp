#include "include/status_badge_function.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"

namespace duckdb {

// Badge constants
static constexpr const char *BADGE_OK = "[ OK ]";
static constexpr const char *BADGE_FAIL = "[FAIL]";
static constexpr const char *BADGE_WARN = "[WARN]";
static constexpr const char *BADGE_RUNNING = "[ .. ]";
static constexpr const char *BADGE_UNKNOWN = "[ ?? ]";

// Overload 1: status_badge(status VARCHAR) -> VARCHAR
// Maps status strings to badge format
static void StatusBadgeFromStringFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &status_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<string_t, string_t>(status_vector, result, count, [&](string_t status) {
		auto status_str = StringUtil::Lower(status.GetString());

		const char *badge;
		if (status_str == "ok" || status_str == "pass" || status_str == "passed" || status_str == "success") {
			badge = BADGE_OK;
		} else if (status_str == "fail" || status_str == "failed" || status_str == "error") {
			badge = BADGE_FAIL;
		} else if (status_str == "warn" || status_str == "warning") {
			badge = BADGE_WARN;
		} else if (status_str == "running" || status_str == "pending" || status_str == "in_progress") {
			badge = BADGE_RUNNING;
		} else {
			badge = BADGE_UNKNOWN;
		}

		return StringVector::AddString(result, badge);
	});
}

// Overload 2: status_badge(error_count INTEGER, warning_count INTEGER, is_running BOOLEAN) -> VARCHAR
// Computes badge from counts
static void StatusBadgeFromCountsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &error_count_vector = args.data[0];
	auto &warning_count_vector = args.data[1];
	auto &is_running_vector = args.data[2];
	auto count = args.size();

	TernaryExecutor::Execute<int64_t, int64_t, bool, string_t>(
	    error_count_vector, warning_count_vector, is_running_vector, result, count,
	    [&](int64_t error_count, int64_t warning_count, bool is_running) {
		    const char *badge;
		    if (is_running) {
			    badge = BADGE_RUNNING;
		    } else if (error_count > 0) {
			    badge = BADGE_FAIL;
		    } else if (warning_count > 0) {
			    badge = BADGE_WARN;
		    } else {
			    badge = BADGE_OK;
		    }

		    return StringVector::AddString(result, badge);
	    });
}

// Overload 3: status_badge(error_count INTEGER, warning_count INTEGER) -> VARCHAR
// Computes badge from counts (assumes not running)
static void StatusBadgeFromCountsNoRunningFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &error_count_vector = args.data[0];
	auto &warning_count_vector = args.data[1];
	auto count = args.size();

	BinaryExecutor::Execute<int64_t, int64_t, string_t>(error_count_vector, warning_count_vector, result, count,
	                                                    [&](int64_t error_count, int64_t warning_count) {
		                                                    const char *badge;
		                                                    if (error_count > 0) {
			                                                    badge = BADGE_FAIL;
		                                                    } else if (warning_count > 0) {
			                                                    badge = BADGE_WARN;
		                                                    } else {
			                                                    badge = BADGE_OK;
		                                                    }

		                                                    return StringVector::AddString(result, badge);
	                                                    });
}

// None of the three overloads is marked SetFallible(): all three are total
// functions over their input (string comparison / integer comparison producing
// one of five constant badges) with no throw, no allocation-dependent failure
// path and no call out of this file. That also keeps this set clear of the
// FunctionSet compat trap — the pinned DuckDB's FunctionSet has `vector<T>
// functions` and no set-level SetFallible, while main's has SetFallible() and
// `vector<shared_ptr<const T>>`, so a set that *did* need marking would need a
// member probe here.
ScalarFunctionSet GetStatusBadgeFunction() {
	ScalarFunctionSet set("status_badge");

	// Overload 1: status_badge(status VARCHAR) -> VARCHAR
	set.AddFunction(ScalarFunction({LogicalType::VARCHAR}, LogicalType::VARCHAR, StatusBadgeFromStringFunction));

	// Overload 2: status_badge(error_count BIGINT, warning_count BIGINT, is_running BOOLEAN) -> VARCHAR
	set.AddFunction(ScalarFunction({LogicalType::BIGINT, LogicalType::BIGINT, LogicalType::BOOLEAN},
	                               LogicalType::VARCHAR, StatusBadgeFromCountsFunction));

	// Overload 3: status_badge(error_count BIGINT, warning_count BIGINT) -> VARCHAR
	set.AddFunction(ScalarFunction({LogicalType::BIGINT, LogicalType::BIGINT}, LogicalType::VARCHAR,
	                               StatusBadgeFromCountsNoRunningFunction));

	return set;
}

} // namespace duckdb
