#pragma once

#include "duckdb.hpp"

// duckdb_compat.hpp — fleet-standard cross-version shim for DuckDB extensions.
//
// Pattern established by @bendrucker in teaguesterling/duckdb_webbed#76 (May 2026):
// detect the new API via __has_include of headers that moved in the same DuckDB
// refactor ([duckdb/duckdb#22377](https://github.com/duckdb/duckdb/pull/22377) —
// "mandatory per-vector size tracking" landed alongside the vector-buffer header
// reshuffle), then dispatch via a single #ifdef block.
//
// Cross-version coverage:
//   - duckdb v1.4.x / v1.5.x: old API everywhere
//   - duckdb main / v1.6.x:   new API everywhere
//
// See teaguesterling/duckdb_markdown's docs/DUCKDB_API_MIGRATION.md for the
// long-form rationale + upgrade checklist for other extensions.

#if __has_include("duckdb/common/vector/list_vector.hpp")
#define DUCKDB_HAS_NEW_VECTOR_HEADERS 1
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/struct_vector.hpp"
#endif

// --- Table function bind: output-names parameter type ---
// duckdb main changed `table_function_bind_t`'s names parameter from
// `vector<string>` to `vector<Identifier>`:
//
//   error: invalid conversion from
//     unique_ptr<FunctionData>(*)(ClientContext&, TableFunctionBindInput&,
//                                 vector<LogicalType>&, vector<string>&)
//   to 'duckdb::table_function_bind_t' {aka
//     unique_ptr<FunctionData>(*)(ClientContext&, TableFunctionBindInput&,
//                                 vector<LogicalType>&, vector<Identifier>&)}
//
// Keyed on identifier.hpp for the same reason webbed_integration.cpp is: v1.5.3
// takes plain strings and does not ship that header. Bind functions declare
// their names parameter as CompatBindNames so one signature compiles on both.
#if __has_include("duckdb/common/identifier.hpp")
#define DUCKDB_HAS_IDENTIFIER_NAMES 1
#include "duckdb/common/identifier.hpp"
#endif

namespace duckdb {
#ifdef DUCKDB_HAS_IDENTIFIER_NAMES
using CompatBindNames = vector<Identifier>;
#else
using CompatBindNames = vector<string>;
#endif

// --- CreateInfo schema/name assignment ---
// duckdb main made CreateInfo's schema/name private behind setters, in the same
// Identifier refactor:
//   error: 'struct duckdb::CreateMacroInfo' has no member named 'schema';
//          did you mean 'SetSchema'?
// Upstream create_info.hpp declares `void SetName(Identifier)` and
// `void SetSchema(Identifier)`; a string literal converts to Identifier
// implicitly, which is the same conversion webbed_integration.cpp relies on for
// DEFAULT_SCHEMA. Keyed on the same probe so the branch cannot disagree with
// CompatBindNames about which API is present.
// Templated on the info type: on the old API `schema` lives on CreateInfo but
// `name` is declared by the derived CreateMacroInfo, so a CreateInfo& parameter
// does not compile there.
template <class INFO>
inline void CompatSetCreateInfoQualification(INFO &info, const char *schema, const char *name) {
#ifdef DUCKDB_HAS_IDENTIFIER_NAMES
	info.SetSchema(schema);
	info.SetName(name);
#else
	info.schema = schema;
	info.name = name;
#endif
}
} // namespace duckdb

namespace duckdb {

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS

// --- Output chunk finalization ---
// DuckDB main mandates per-vector Size() tracking; DataChunk::SetCardinality only
// updates chunk.count. SetChildCardinality additionally calls FlatVector::SetSize
// on every column so query operators reading vec.Size() see the right value.
// Without this, VariadicExecutor (and similar) reports:
//   "Mismatch in input vector sizes ... expected 0 rows but got N"
inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	chunk.SetChildCardinality(count);
}

#else // Old API (v1.4.x / v1.5.x)

inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	chunk.SetCardinality(count);
}

#endif

} // namespace duckdb
