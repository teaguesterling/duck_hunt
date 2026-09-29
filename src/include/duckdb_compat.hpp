#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"

#include <type_traits>

// duckdb_compat.hpp — fleet-standard cross-version shim for DuckDB extensions.
//
// Cross-version coverage:
//   - duckdb v1.4.x / v1.5.x: old API everywhere
//   - duckdb main / v2.0.x:   new API everywhere
//
// RULE: detect features, not versions — and probe the *thing itself*, never a
// proxy for it. `__has_include` answers exactly one question ("should I
// #include this header?") and is never allowed to decide a type or a member
// call. DuckDB backports headers to the stable branch ahead of the behaviour
// they belong to, so a header probe is a leading indicator, not a test:
//
//   v1.5-variegata @ b155d6f63c (the pin)   no identifier.hpp   bind: vector<string>
//   v1.5-variegata @ branch TIP             HAS identifier.hpp  bind: vector<string>
//   main (v2.0)                             HAS identifier.hpp  bind: vector<Identifier>
//
// A header-keyed `CompatBindNames` is correct on the pin only by accident and
// breaks the *shipped* build on the next submodule bump. Everything below is
// therefore derived from the DuckDB declaration that actually changed, or
// selected by a member probe.
//
// NOTE: these TUs compile at -std=c++11 (verified via
// `grep -o 'std=c++[0-9]*' build/release/compile_commands.json`), so dispatch is
// written as tag dispatch rather than `if constexpr`. Every `*Impl` overload
// must be a template — as plain functions both bodies would be compiled and the
// v2.0-only branch would break the v1.5 build.

// Header includes only. These MUST NOT gate any type or member selection.
#if __has_include("duckdb/common/vector/list_vector.hpp")
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/struct_vector.hpp"
#endif

#if __has_include("duckdb/common/identifier.hpp")
#include "duckdb/common/identifier.hpp"
#define DUCKDB_HAS_IDENTIFIER 1
#endif

namespace duckdb {

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
// Ask DuckDB what its own bind signature says instead of guessing from a
// header: the 4th parameter of `table_function_bind_t` IS the thing that
// changed, so this cannot drift. Bind functions declare their names parameter
// as `CompatBindNames &` and one signature compiles on both lines.
//
// String *literals* need no helper — `Identifier(const char *)` is implicit —
// which is why every `names = {...}` / `names.push_back("context")` site in this
// extension is unchanged. Only a *runtime* string crossing the boundary would
// need an explicit conversion, and duck_hunt has none.
template <class T>
struct CompatBindNamesOf;

template <class R, class A, class B, class C, class D>
struct CompatBindNamesOf<R (*)(A, B, C, D)> {
	// `typename` is REQUIRED here: D is dependent.
	using type = typename std::remove_reference<D>::type::value_type;
};

// No `typename` needed at namespace scope — the name is not dependent.
using CompatBindNames = vector<CompatBindNamesOf<table_function_bind_t>::type>;

// --- CreateInfo schema/name assignment ---
// duckdb main made CreateInfo's schema/name private behind setters:
//   error: 'struct duckdb::CreateMacroInfo' has no member named 'schema';
//          did you mean 'SetSchema'?
// Upstream create_info.hpp declares `void SetName(Identifier)` and
// `void SetSchema(Identifier)`; a string literal converts to Identifier
// implicitly. This is a question about a *member*, so it is answered by a
// member probe — not by identifier.hpp's presence, which can be backported
// ahead of the accessors and would then break the pinned build.
//
// Templated on the info type: on the old API `schema` lives on CreateInfo but
// `name` is declared by the derived CreateMacroInfo, so a CreateInfo& parameter
// does not compile there.
template <class T, class = void>
struct CompatHasSetSchema : std::false_type {};

template <class T>
struct CompatHasSetSchema<T, decltype(void(std::declval<T &>().SetSchema(std::declval<const char *>())))>
    : std::true_type {};

template <class INFO>
inline void CompatSetCreateInfoQualificationImpl(INFO &info, const char *schema, const char *name, std::true_type) {
	info.SetSchema(schema); // v2.0
	info.SetName(name);
}

template <class INFO>
inline void CompatSetCreateInfoQualificationImpl(INFO &info, const char *schema, const char *name, std::false_type) {
	info.schema = schema; // v1.5
	info.name = name;
}

template <class INFO>
inline void CompatSetCreateInfoQualification(INFO &info, const char *schema, const char *name) {
	CompatSetCreateInfoQualificationImpl(info, schema, name, CompatHasSetSchema<INFO>());
}

// --- QueryResult column-names accessor ---
// TWO independent things changed between the lines, and conflating them is
// exactly the partial-backport trap:
//   1. ACCESS PATH: duckdb main made BaseQueryResult::names private behind
//      GetNames(); v1.5.x keeps `names` public with no accessor.
//   2. ELEMENT TYPE: on v1.5.x it is vector<string>; on v2.0
//      GetNames() returns `const vector<Identifier> &` (verified against
//      duckdb main query_result.hpp:43,65 — same Identifier refactor as
//      CompatBindNames above).
// v1.5.6 backports identifier.hpp while keeping `names` public AND string, so
// a header-keyed probe (`__has_include`) is a false positive on BOTH axes.
// Select the access path by a member probe of GetNames() itself, and
// normalize the element type with CompatNameStr — never gate either on the
// header. Returns vector<string> BY VALUE (one return type across all lines;
// this is a cold test-framework path, so the copy is irrelevant).
//
// String-passthrough / Identifier::GetIdentifierName() extractor. The
// Identifier overload is only *declared* when identifier.hpp is present (you
// cannot name a type that does not exist); it never *selects* the branch —
// overload resolution does that on the real element type, so on v1.5.6 (header
// present, elements still string) the string overload still wins.
inline string CompatNameStr(const string &name) {
	return name;
}
#ifdef DUCKDB_HAS_IDENTIFIER
inline string CompatNameStr(const Identifier &id) {
	return id.GetIdentifierName();
}
#endif

template <class T, class = void>
struct CompatHasGetNames : std::false_type {};

template <class T>
struct CompatHasGetNames<T, decltype(void(std::declval<T &>().GetNames()))>
    : std::true_type {};

// c++11: no auto-return / if constexpr, so tag dispatch; every *Impl is a
// template so the v2.0-only GetNames() body is not instantiated on the v1.5 build.
template <class RESULT>
inline vector<string> CompatResultNamesImpl(RESULT &result, std::true_type) { // v2.0
	vector<string> out;
	for (auto &n : result.GetNames()) {
		out.push_back(CompatNameStr(n));
	}
	return out;
}

template <class RESULT>
inline vector<string> CompatResultNamesImpl(RESULT &result, std::false_type) { // v1.5
	vector<string> out;
	for (auto &n : result.names) {
		out.push_back(CompatNameStr(n));
	}
	return out;
}

template <class RESULT>
inline vector<string> CompatResultNames(RESULT &result) {
	return CompatResultNamesImpl(result, CompatHasGetNames<RESULT>());
}

// --- Output chunk finalization ---
// DuckDB main mandates per-vector Size() tracking; DataChunk::SetCardinality only
// updates chunk.count. SetChildCardinality additionally calls FlatVector::SetSize
// on every column so query operators reading vec.Size() see the right value.
// Without this, VariadicExecutor (and similar) reports:
//   "Mismatch in input vector sizes ... expected 0 rows but got N"
//
// duck_hunt writes output vectors positionally and never calls the appending
// Vector APIs (audited: zero such call sites in src/), so SetChildCardinality
// is the correct choice on the new API. Selection is by
// member probe rather than by `__has_include("duckdb/common/vector/list_vector.hpp")`:
// that header can be backported without SetChildCardinality coming along, which
// would break the *pinned* build at the next submodule bump.
template <class T, class = void>
struct CompatHasSetChildCardinality : std::false_type {};

template <class T>
struct CompatHasSetChildCardinality<T, decltype(void(std::declval<T &>().SetChildCardinality(idx_t(0))))>
    : std::true_type {};

template <class CHUNK>
inline void CompatSetOutputCardinalityImpl(CHUNK &chunk, idx_t count, std::true_type) {
	chunk.SetChildCardinality(count); // v2.0
}

template <class CHUNK>
inline void CompatSetOutputCardinalityImpl(CHUNK &chunk, idx_t count, std::false_type) {
	chunk.SetCardinality(count); // v1.5
}

inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	CompatSetOutputCardinalityImpl(chunk, count, CompatHasSetChildCardinality<DataChunk>());
}

} // namespace duckdb
