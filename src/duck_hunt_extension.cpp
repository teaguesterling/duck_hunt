#define DUCKDB_EXTENSION_MAIN

#include "duck_hunt_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

// Duck Hunt specific includes
#include "include/read_test_results_function.hpp"
#include "include/read_workflow_logs_function.hpp"
#include "include/parse_workflow_logs_function.hpp"
#include "include/validation_event_types.hpp"
#include "core/parser_registry.hpp"

// Include parser headers to force registration
#include "parsers/tool_outputs/eslint_json_parser.hpp"
#include "parsers/tool_outputs/gotest_json_parser.hpp"
#include "parsers/test_frameworks/pytest_json_parser.hpp"
#include "parsers/build_systems/make_parser.hpp"
#include "parsers/linting_tools/mypy_parser.hpp"
#include "parsers/linting_tools/black_parser.hpp"
#include "parsers/linting_tools/flake8_parser.hpp"
#include "parsers/linting_tools/pylint_parser.hpp"
#include "parsers/tool_outputs/rubocop_json_parser.hpp"
#include "parsers/tool_outputs/cargo_test_json_parser.hpp"
#include "parsers/tool_outputs/swiftlint_json_parser.hpp"
#include "parsers/tool_outputs/phpstan_json_parser.hpp"
#include "parsers/tool_outputs/shellcheck_json_parser.hpp"
#include "parsers/tool_outputs/stylelint_json_parser.hpp"
#include "parsers/tool_outputs/clippy_json_parser.hpp"
#include "parsers/tool_outputs/markdownlint_json_parser.hpp"
#include "parsers/tool_outputs/yamllint_json_parser.hpp"
#include "parsers/tool_outputs/spotbugs_json_parser.hpp"
#include "parsers/tool_outputs/ktlint_json_parser.hpp"
#include "parsers/tool_outputs/bandit_json_parser.hpp"
#include "parsers/tool_outputs/kubescore_json_parser.hpp"
#include "parsers/ci_systems/drone_ci_text_parser.hpp"
#include "parsers/ci_systems/terraform_text_parser.hpp"
#include "parsers/infrastructure_tools/ansible_text_parser.hpp"
#include "parsers/linting_tools/yapf_text_parser.hpp"

// Phase 3: Workflow Engine parsers  
#include "parsers/workflow_engines/github_actions_parser.hpp"
#include "parsers/workflow_engines/gitlab_ci_parser.hpp"
#include "parsers/workflow_engines/jenkins_parser.hpp"
#include "parsers/workflow_engines/docker_parser.hpp"

// Include workflow engine interface for registry
#include "workflow_engine_interface.hpp"

namespace duckdb {

static void LoadInternal(ExtensionLoader &loader) {
	// DEBUG: Test if LoadInternal is being called
	// throw InternalException("DEBUG: LoadInternal was called");
	
	// Initialize parser registry with key parsers
	auto& registry = ParserRegistry::getInstance();
	registry.registerParser(make_uniq<ESLintJSONParser>());
	registry.registerParser(make_uniq<GoTestJSONParser>());
	registry.registerParser(make_uniq<PytestJSONParser>());
	registry.registerParser(make_uniq<MakeParser>());
	registry.registerParser(make_uniq<MypyParser>());
	registry.registerParser(make_uniq<BlackParser>());
	registry.registerParser(make_uniq<Flake8Parser>());
	registry.registerParser(make_uniq<PylintParser>());
	registry.registerParser(make_uniq<RuboCopJSONParser>());
	registry.registerParser(make_uniq<CargoTestJSONParser>());
	registry.registerParser(make_uniq<SwiftLintJSONParser>());
	registry.registerParser(make_uniq<PHPStanJSONParser>());
	registry.registerParser(make_uniq<ShellCheckJSONParser>());
	registry.registerParser(make_uniq<StylelintJSONParser>());
	registry.registerParser(make_uniq<ClippyJSONParser>());
	registry.registerParser(make_uniq<MarkdownlintJSONParser>());
	registry.registerParser(make_uniq<YamllintJSONParser>());
	registry.registerParser(make_uniq<SpotBugsJSONParser>());
	registry.registerParser(make_uniq<KtlintJSONParser>());
	registry.registerParser(make_uniq<BanditJSONParser>());
	registry.registerParser(make_uniq<KubeScoreJSONParser>());
	registry.registerParser(make_uniq<DroneCITextParser>());
	registry.registerParser(make_uniq<TerraformTextParser>());
	registry.registerParser(make_uniq<AnsibleTextParser>());
	registry.registerParser(make_uniq<YapfTextParser>());
	
	// Register table functions for test result parsing
	auto read_test_results_function = GetReadTestResultsFunction();
	loader.RegisterFunction(read_test_results_function);
	
	auto parse_test_results_function = GetParseTestResultsFunction();
	loader.RegisterFunction(parse_test_results_function);
	
	// Phase 3: Register workflow log parsing functions
	auto read_workflow_logs_function = GetReadWorkflowLogsFunction();
	loader.RegisterFunction(read_workflow_logs_function);
	
	auto parse_workflow_logs_function = GetParseWorkflowLogsFunction();
	loader.RegisterFunction(parse_workflow_logs_function);
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
