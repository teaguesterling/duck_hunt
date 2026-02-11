#include "core/parser_registry.hpp"
#include "parsers/base/base_parser.hpp"
#include "pylint_parser.hpp"
#include "mypy_parser.hpp"
#include "flake8_parser.hpp"
#include "black_parser.hpp"
#include "yapf_text_parser.hpp"
#include "clang_tidy_parser.hpp"
#include "autopep8_text_parser.hpp"
#include "isort_parser.hpp"
#include "bandit_text_parser.hpp"
#include "ruff_parser.hpp"
#include "eslint_text_parser.hpp"
#include "rubocop_text_parser.hpp"
#include "shellcheck_text_parser.hpp"
#include "hadolint_text_parser.hpp"

namespace duckdb {

/**
 * Register all linting tool parsers with the registry.
 * Uses DelegatingParser template to reduce boilerplate.
 */
DECLARE_PARSER_CATEGORY(LintingTools);

void RegisterLintingToolsParsers(ParserRegistry &registry) {
	// Format: format_name, display_name, category, description, priority, aliases, groups
	registry.registerParser(make_uniq<DelegatingParser<PylintParser>>(
	    "pylint_text", "Pylint Parser", ParserCategory::LINTING, "Python Pylint code quality output",
	    ParserPriority::HIGH, std::vector<std::string> {"pylint"}, std::vector<std::string> {"python", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<MypyParser>>(
	    "mypy_text", "Mypy Parser", ParserCategory::LINTING, "Python Mypy type checker output", ParserPriority::HIGH,
	    std::vector<std::string> {"mypy"}, std::vector<std::string> {"python", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<Flake8Parser>>(
	    "flake8_text", "Flake8 Parser", ParserCategory::LINTING, "Python Flake8 style checker output",
	    ParserPriority::HIGH, std::vector<std::string> {"flake8"}, std::vector<std::string> {"python", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<BlackParser>>(
	    "black_text", "Black Parser", ParserCategory::LINTING, "Python Black formatter output", ParserPriority::MEDIUM,
	    std::vector<std::string> {"black"}, std::vector<std::string> {"python", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<YapfTextParser>>(
	    "yapf_text", "YAPF Parser", ParserCategory::LINTING, "Python YAPF formatter output", ParserPriority::VERY_HIGH,
	    std::vector<std::string> {"yapf"}, std::vector<std::string> {"python", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<ClangTidyParser>>(
	    "clang_tidy_text", "Clang-Tidy Parser", ParserCategory::LINTING, "LLVM Clang-Tidy C++ linter output",
	    ParserPriority::HIGH, std::vector<std::string> {"clang_tidy", "clang-tidy"},
	    std::vector<std::string> {"c_cpp", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<Autopep8TextParser>>(
	    "autopep8_text", "Autopep8 Parser", ParserCategory::LINTING, "Python autopep8 formatter output",
	    ParserPriority::HIGH, std::vector<std::string> {"autopep8"}, std::vector<std::string> {"python", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<IsortParser>>(
	    "isort_text", "isort Parser", ParserCategory::LINTING, "Python isort import sorter output",
	    ParserPriority::HIGH, std::vector<std::string> {"isort"}, std::vector<std::string> {"python", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<BanditTextParser>>(
	    "bandit_text", "Bandit Parser", ParserCategory::LINTING, "Python Bandit security linter output",
	    ParserPriority::HIGH, std::vector<std::string> {"bandit"},
	    std::vector<std::string> {"python", "lint", "security"}));

	registry.registerParser(make_uniq<DelegatingParser<RuffParser>>(
	    "ruff_text", "Ruff Parser", ParserCategory::LINTING, "Python Ruff linter output", ParserPriority::VERY_HIGH,
	    std::vector<std::string> {"ruff"}, std::vector<std::string> {"python", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<EslintTextParser>>(
	    "eslint_text", "ESLint Text Parser", ParserCategory::LINTING, "ESLint JavaScript/TypeScript linter text output",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"javascript", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<RubocopTextParser>>(
	    "rubocop_text", "RuboCop Text Parser", ParserCategory::LINTING, "RuboCop Ruby linter text output",
	    ParserPriority::HIGH, std::vector<std::string> {}, std::vector<std::string> {"ruby", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<ShellcheckTextParser>>(
	    "shellcheck_text", "ShellCheck Text Parser", ParserCategory::LINTING,
	    "ShellCheck shell script linter text output", ParserPriority::HIGH, std::vector<std::string> {},
	    std::vector<std::string> {"shell", "lint"}));

	registry.registerParser(make_uniq<DelegatingParser<HadolintTextParser>>(
	    "hadolint_text", "Hadolint Text Parser", ParserCategory::LINTING, "Hadolint Dockerfile linter text output",
	    ParserPriority::HIGH, std::vector<std::string> {},
	    std::vector<std::string> {"docker", "lint", "infrastructure"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(LintingTools);

} // namespace duckdb
