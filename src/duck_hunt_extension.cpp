#define DUCKDB_EXTENSION_MAIN

#include "duck_hunt_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include <duckdb/parser/parsed_data/create_table_function_info.hpp>

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
	{
		auto read_duck_hunt_log_function = GetReadDuckHuntLogFunction();
		CreateTableFunctionInfo info(std::move(read_duck_hunt_log_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"path", "format"};
		desc.description = "Read and parse test and validation logs from files into structured validation events.";
		desc.examples = {"SELECT * FROM read_duck_hunt_log('test.log')"};
		desc.categories = {"duck_hunt", "logs"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	{
		auto parse_duck_hunt_log_function = GetParseDuckHuntLogFunction();
		CreateTableFunctionInfo info(std::move(parse_duck_hunt_log_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"content", "format"};
		desc.description = "Parse test and validation log text content into structured validation events.";
		desc.examples = {"SELECT * FROM parse_duck_hunt_log('PASSED: test_foo')"};
		desc.categories = {"duck_hunt", "logs"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	// Phase 3: Register workflow log parsing functions
	{
		auto read_duck_hunt_workflow_log_function = GetReadDuckHuntWorkflowLogFunction();
		CreateTableFunctionInfo info(std::move(read_duck_hunt_workflow_log_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"path", "format"};
		desc.description = "Read and parse workflow logs (GitHub Actions, GitLab CI, etc.) into steps and events.";
		desc.examples = {"SELECT * FROM read_duck_hunt_workflow_log('workflow.log')"};
		desc.categories = {"duck_hunt", "ci"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	{
		auto parse_duck_hunt_workflow_log_function = GetParseDuckHuntWorkflowLogFunction();
		CreateTableFunctionInfo info(std::move(parse_duck_hunt_workflow_log_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"content", "format"};
		desc.description = "Parse workflow log text into structured steps and events.";
		desc.examples = {"SELECT * FROM parse_duck_hunt_workflow_log('##[group]Run step')"};
		desc.categories = {"duck_hunt", "ci"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	// Register scalar utility functions
	{
		auto status_badge_function = GetStatusBadgeFunction();
		CreateScalarFunctionInfo info(std::move(status_badge_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc1;
		desc1.parameter_types = {LogicalType::VARCHAR};
		desc1.parameter_names = {"status"};
		desc1.description = "Generate a status badge string from test execution status.";
		desc1.examples = {"status_badge('passed')"};
		desc1.categories = {"duck_hunt"};
		info.descriptions.push_back(desc1);

		FunctionDescription desc2;
		desc2.parameter_types = {LogicalType::BIGINT, LogicalType::BIGINT, LogicalType::BIGINT};
		desc2.parameter_names = {"passed", "failed", "skipped"};
		desc2.description = "Generate a status badge string from passed, failed, and skipped test counts.";
		desc2.examples = {"status_badge(10, 0, 1)"};
		desc2.categories = {"duck_hunt"};
		info.descriptions.push_back(desc2);
		loader.RegisterFunction(std::move(info));
	}

	// Register format discovery function
	{
		auto duck_hunt_formats_function = GetDuckHuntFormatsFunction();
		CreateTableFunctionInfo info(std::move(duck_hunt_formats_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.description = "List all supported log and test output formats in duck_hunt.";
		desc.examples = {"SELECT * FROM duck_hunt_formats()"};
		desc.categories = {"duck_hunt"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	// Register diagnostic functions
	{
		auto duck_hunt_diagnose_parse_function = GetDuckHuntDiagnoseParseFunction();
		CreateTableFunctionInfo info(std::move(duck_hunt_diagnose_parse_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"content", "emit"};
		desc.description = "Diagnose log parsing issues for string content across all known format parsers.";
		desc.examples = {"SELECT * FROM duck_hunt_diagnose_parse('sample log content')"};
		desc.categories = {"duck_hunt"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	{
		auto duck_hunt_diagnose_read_function = GetDuckHuntDiagnoseReadFunction();
		CreateTableFunctionInfo info(std::move(duck_hunt_diagnose_read_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"path", "emit"};
		desc.description = "Diagnose log parsing issues for a log file path across all known format parsers.";
		desc.examples = {"SELECT * FROM duck_hunt_diagnose_read('build.log')"};
		desc.categories = {"duck_hunt"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	// Register format detection scalar function
	{
		auto duck_hunt_detect_format_function = GetDuckHuntDetectFormatFunction();
		CreateScalarFunctionInfo info(std::move(duck_hunt_detect_format_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"content"};
		desc.description = "Detect the log format of a text sample.";
		desc.examples = {"duck_hunt_detect_format('=== RUN TestFoo')"};
		desc.categories = {"duck_hunt"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	// Register table macros
	RegisterDuckHuntMacros(loader);

	// Register custom parser configuration functions
	{
		auto load_parser_config_function = GetDuckHuntLoadParserConfigFunction();
		CreateScalarFunctionInfo info(std::move(load_parser_config_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"json_config"};
		desc.description = "Load a dynamic JSON parser configuration into duck_hunt.";
		desc.examples = {"duck_hunt_load_parser_config('{}')"};
		desc.categories = {"duck_hunt"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}

	{
		auto unload_parser_function = GetDuckHuntUnloadParserFunction();
		CreateScalarFunctionInfo info(std::move(unload_parser_function));
		info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
		FunctionDescription desc;
		desc.parameter_names = {"format_name"};
		desc.description = "Unload a dynamic parser from duck_hunt.";
		desc.examples = {"duck_hunt_unload_parser('custom_fmt')"};
		desc.categories = {"duck_hunt"};
		info.descriptions.push_back(desc);
		loader.RegisterFunction(std::move(info));
	}
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
