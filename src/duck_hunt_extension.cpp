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
	
	// Register table functions for test result parsing
	auto read_test_results_function = GetReadTestResultsFunction();
	ExtensionUtil::RegisterFunction(instance, read_test_results_function);
	
	auto parse_test_results_function = GetParseTestResultsFunction();
	ExtensionUtil::RegisterFunction(instance, parse_test_results_function);
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
