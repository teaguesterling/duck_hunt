#define DUCKDB_EXTENSION_MAIN

#include "duck_hunt_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

// Duck Hunt specific includes
#include "include/read_duck_hunt_log_function.hpp"
#include "include/read_duck_hunt_workflow_log_function.hpp"
#include "include/parse_duck_hunt_workflow_log_function.hpp"
#include "include/validation_event_types.hpp"
#include "include/status_badge_function.hpp"
#include "include/duck_hunt_formats_function.hpp"
#include "include/duck_hunt_diagnose_function.hpp"
#include "include/duck_hunt_detect_format_function.hpp"
#include "include/duck_hunt_macros.hpp"
#include "include/config_parser_functions.hpp"
#include "core/parser_registry.hpp" // Modular parser registry

// Workflow engine interface for registry
#include "workflow_engine_interface.hpp"

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
	// Initialize modular parser registry (category-based auto-registration)
	InitializeAllParsers();

	// Register table functions for test result parsing
	auto read_duck_hunt_log_function = GetReadDuckHuntLogFunction();
	loader.RegisterFunction(read_duck_hunt_log_function);

	auto parse_duck_hunt_log_function = GetParseDuckHuntLogFunction();
	loader.RegisterFunction(parse_duck_hunt_log_function);

	// Phase 3: Register workflow log parsing functions
	auto read_duck_hunt_workflow_log_function = GetReadDuckHuntWorkflowLogFunction();
	loader.RegisterFunction(read_duck_hunt_workflow_log_function);

	auto parse_duck_hunt_workflow_log_function = GetParseDuckHuntWorkflowLogFunction();
	loader.RegisterFunction(parse_duck_hunt_workflow_log_function);

	// Register scalar utility functions
	auto status_badge_function = GetStatusBadgeFunction();
	loader.RegisterFunction(status_badge_function);

	// Register format discovery function
	auto duck_hunt_formats_function = GetDuckHuntFormatsFunction();
	loader.RegisterFunction(duck_hunt_formats_function);

	// Register diagnostic functions
	auto duck_hunt_diagnose_parse_function = GetDuckHuntDiagnoseParseFunction();
	loader.RegisterFunction(duck_hunt_diagnose_parse_function);

	auto duck_hunt_diagnose_read_function = GetDuckHuntDiagnoseReadFunction();
	loader.RegisterFunction(duck_hunt_diagnose_read_function);

	// Register format detection scalar function
	auto duck_hunt_detect_format_function = GetDuckHuntDetectFormatFunction();
	loader.RegisterFunction(duck_hunt_detect_format_function);

	// Register table macros
	RegisterDuckHuntMacros(loader);

	// Register custom parser configuration functions
	auto load_parser_config_function = GetDuckHuntLoadParserConfigFunction();
	loader.RegisterFunction(load_parser_config_function);

	auto unload_parser_function = GetDuckHuntUnloadParserFunction();
	loader.RegisterFunction(unload_parser_function);
}

void DuckHuntExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
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

DUCKDB_CPP_EXTENSION_ENTRY(duck_hunt, loader) {
	duckdb::LoadInternal(loader);
}

DUCKDB_EXTENSION_API const char *duck_hunt_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
