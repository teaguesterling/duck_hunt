#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "eslint_json_parser.hpp"
#include "gotest_json_parser.hpp"
#include "rubocop_json_parser.hpp"
#include "cargo_test_json_parser.hpp"
#include "swiftlint_json_parser.hpp"
#include "phpstan_json_parser.hpp"
#include "shellcheck_json_parser.hpp"
#include "stylelint_json_parser.hpp"
#include "clippy_json_parser.hpp"
#include "markdownlint_json_parser.hpp"
#include "yamllint_json_parser.hpp"
#include "bandit_json_parser.hpp"
#include "spotbugs_json_parser.hpp"
#include "ktlint_json_parser.hpp"
#include "kubescore_json_parser.hpp"
#include "hadolint_json_parser.hpp"
#include "lintr_json_parser.hpp"
#include "sqlfluff_json_parser.hpp"
#include "tflint_json_parser.hpp"
#include "trivy_json_parser.hpp"
#include "tfsec_json_parser.hpp"
#include "generic_lint_parser.hpp"

namespace duckdb {

// Alias for convenience
template <typename T>
using P = DelegatingParser<T>;

/**
 * Register all tool output parsers with the registry.
 */
DECLARE_PARSER_CATEGORY(ToolOutputs);

void RegisterToolOutputsParsers(ParserRegistry &registry) {
	// Linting tools - JavaScript/TypeScript
	registry.registerParser(make_uniq<P<ESLintJSONParser>>(
	    "eslint_json", "ESLint JSON Parser", ParserCategory::LINTING, "ESLint JavaScript/TypeScript linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"eslint"}, std::vector<std::string> {"javascript", "lint"}));

	registry.registerParser(make_uniq<P<StylelintJSONParser>>(
	    "stylelint_json", "Stylelint JSON Parser", ParserCategory::LINTING, "Stylelint CSS linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"stylelint"}, std::vector<std::string> {"javascript", "lint"}));

	// Linting tools - Ruby
	registry.registerParser(make_uniq<P<RuboCopJSONParser>>(
	    "rubocop_json", "RuboCop JSON Parser", ParserCategory::LINTING, "RuboCop Ruby linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"rubocop"}, std::vector<std::string> {"ruby", "lint"}));

	// Linting tools - Swift
	registry.registerParser(make_uniq<P<SwiftLintJSONParser>>(
	    "swiftlint_json", "SwiftLint JSON Parser", ParserCategory::LINTING, "SwiftLint Swift linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"swiftlint"}, std::vector<std::string> {"swift", "lint"}));

	// Linting tools - PHP
	registry.registerParser(make_uniq<P<PHPStanJSONParser>>(
	    "phpstan_json", "PHPStan JSON Parser", ParserCategory::LINTING, "PHPStan PHP static analysis JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"phpstan"}, std::vector<std::string> {"php", "lint"}));

	// Linting tools - Shell
	registry.registerParser(
	    make_uniq<P<ShellCheckJSONParser>>("shellcheck_json", "ShellCheck JSON Parser", ParserCategory::LINTING,
	                                       "ShellCheck shell script linter JSON output", ParserPriority::VERY_HIGH,
	                                       std::vector<std::string> {"shellcheck"}, std::vector<std::string> {"shell", "lint"}));

	// Linting tools - Rust
	registry.registerParser(make_uniq<P<ClippyJSONParser>>("clippy_json", "Clippy JSON Parser", ParserCategory::LINTING,
	                                                       "Rust Clippy linter JSON output", ParserPriority::VERY_HIGH,
	                                                       std::vector<std::string> {"clippy"}, std::vector<std::string> {"rust", "lint"}));

	// Linting tools - Config files
	registry.registerParser(
	    make_uniq<P<MarkdownlintJSONParser>>("markdownlint_json", "Markdownlint JSON Parser", ParserCategory::LINTING,
	                                         "Markdownlint markdown linter JSON output", ParserPriority::VERY_HIGH,
	                                         std::vector<std::string> {"markdownlint"}, std::vector<std::string> {"lint"}));

	registry.registerParser(make_uniq<P<YamllintJSONParser>>(
	    "yamllint_json", "Yamllint JSON Parser", ParserCategory::LINTING, "Yamllint YAML linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"yamllint"}, std::vector<std::string> {"lint"}));

	// Linting tools - Java/JVM
	registry.registerParser(make_uniq<P<SpotBugsJSONParser>>(
	    "spotbugs_json", "SpotBugs JSON Parser", ParserCategory::LINTING, "SpotBugs Java static analysis JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"spotbugs"}, std::vector<std::string> {"java", "lint", "security"}));

	registry.registerParser(make_uniq<P<KtlintJSONParser>>(
	    "ktlint_json", "Ktlint JSON Parser", ParserCategory::LINTING, "Ktlint Kotlin linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"ktlint"}, std::vector<std::string> {"java", "lint"}));

	// Linting tools - Docker
	registry.registerParser(make_uniq<P<HadolintJSONParser>>(
	    "hadolint_json", "Hadolint JSON Parser", ParserCategory::LINTING, "Hadolint Dockerfile linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"hadolint"}, std::vector<std::string> {"infrastructure", "lint"}));

	// Linting tools - R
	registry.registerParser(make_uniq<P<LintrJSONParser>>("lintr_json", "Lintr JSON Parser", ParserCategory::LINTING,
	                                                      "Lintr R linter JSON output", ParserPriority::VERY_HIGH,
	                                                      std::vector<std::string> {"lintr"}, std::vector<std::string> {"lint"}));

	// Linting tools - SQL
	registry.registerParser(make_uniq<P<SqlfluffJSONParser>>(
	    "sqlfluff_json", "SQLFluff JSON Parser", ParserCategory::LINTING, "SQLFluff SQL linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"sqlfluff"}, std::vector<std::string> {"lint"}));

	// Linting tools - Infrastructure
	registry.registerParser(make_uniq<P<TflintJSONParser>>(
	    "tflint_json", "TFLint JSON Parser", ParserCategory::LINTING, "Terraform TFLint linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"tflint"}, std::vector<std::string> {"infrastructure", "lint"}));

	registry.registerParser(make_uniq<P<KubeScoreJSONParser>>(
	    "kube_score_json", "Kube-score JSON Parser", ParserCategory::LINTING,
	    "Kube-score Kubernetes manifest linter JSON output", ParserPriority::VERY_HIGH,
	    std::vector<std::string> {"kubescore", "kube_score"}, std::vector<std::string> {"infrastructure", "lint"}));

	// Security tools
	registry.registerParser(make_uniq<P<BanditJSONParser>>(
	    "bandit_json", "Bandit JSON Parser", ParserCategory::SECURITY_TOOL, "Bandit Python security linter JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"bandit"}, std::vector<std::string> {"python", "security", "lint"}));

	registry.registerParser(
	    make_uniq<P<TrivyJSONParser>>("trivy_json", "Trivy JSON Parser", ParserCategory::SECURITY_TOOL,
	                                  "Trivy container/dependency vulnerability scanner JSON output",
	                                  ParserPriority::VERY_HIGH, std::vector<std::string> {"trivy"}, std::vector<std::string> {"infrastructure", "security"}));

	registry.registerParser(make_uniq<P<TfsecJSONParser>>(
	    "tfsec_json", "tfsec JSON Parser", ParserCategory::SECURITY_TOOL,
	    "tfsec Terraform security scanner JSON output", ParserPriority::VERY_HIGH, std::vector<std::string> {"tfsec"}, std::vector<std::string> {"infrastructure", "security"}));

	// Test frameworks
	registry.registerParser(make_uniq<P<GoTestJSONParser>>(
	    "gotest_json", "Go Test JSON Parser", ParserCategory::TEST_FRAMEWORK, "Go test JSON output (go test -json)",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"gotest", "go_test"}, std::vector<std::string> {"go", "test"}));

	registry.registerParser(make_uniq<P<CargoTestJSONParser>>(
	    "cargo_test_json", "Cargo Test JSON Parser", ParserCategory::TEST_FRAMEWORK, "Rust cargo test JSON output",
	    ParserPriority::VERY_HIGH, std::vector<std::string> {"cargo_test"}, std::vector<std::string> {"rust", "test"}));

	// Generic fallback (low priority)
	registry.registerParser(make_uniq<P<GenericLintParser>>(
	    "generic_lint", "Generic Lint Parser", ParserCategory::LINTING,
	    "Generic lint format (file:line:col: level: message)", ParserPriority::LOW, std::vector<std::string> {"lint"}, std::vector<std::string> {"lint"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(ToolOutputs);

} // namespace duckdb
