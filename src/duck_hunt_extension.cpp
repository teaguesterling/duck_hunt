#define DUCKDB_EXTENSION_MAIN

#include "duck_hunt_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

// Duck Hunt specific includes
#include "include/read_test_results_function.hpp"
#include "include/validation_event_types.hpp"

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>

namespace duckdb {

inline void DuckHuntScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "DuckHunt " + name.GetString() + " 🐥");
	});
}

inline void DuckHuntOpenSSLVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "DuckHunt " + name.GetString() + ", my linked OpenSSL version is " +
		                                           OPENSSL_VERSION_TEXT);
	});
}

static void LoadInternal(DatabaseInstance &instance) {
	// Register original scalar functions
	auto duck_hunt_scalar_function = ScalarFunction("duck_hunt", {LogicalType::VARCHAR}, LogicalType::VARCHAR, DuckHuntScalarFun);
	ExtensionUtil::RegisterFunction(instance, duck_hunt_scalar_function);

	auto duck_hunt_openssl_version_scalar_function = ScalarFunction("duck_hunt_openssl_version", {LogicalType::VARCHAR},
	                                                            LogicalType::VARCHAR, DuckHuntOpenSSLVersionScalarFun);
	ExtensionUtil::RegisterFunction(instance, duck_hunt_openssl_version_scalar_function);

	// Register the main table function for test result parsing
	auto read_test_results_function = GetReadTestResultsFunction();
	ExtensionUtil::RegisterFunction(instance, read_test_results_function);
}

void DuckHuntExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}
std::string DuckHuntExtension::Name() {
	return "duck_hunt";
}

std::string DuckHuntExtension::Version() const {
#ifdef EXT_VERSION_DUCK_HUNT
	return EXT_VERSION_DUCK_HUNT;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void duck_hunt_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::DuckHuntExtension>();
}

DUCKDB_EXTENSION_API const char *duck_hunt_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
