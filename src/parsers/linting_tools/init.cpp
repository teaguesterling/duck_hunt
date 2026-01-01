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

namespace duckdb {

/**
 * Register all linting tool parsers with the registry.
 * Uses DelegatingParser template to reduce boilerplate.
 */
DECLARE_PARSER_CATEGORY(LintingTools);

void RegisterLintingToolsParsers(ParserRegistry &registry) {
	registry.registerParser(make_uniq<DelegatingParser<PylintParser>>(
	    "pylint_text", "Pylint Parser", ParserCategory::LINTING, "Python Pylint code quality output",
	    ParserPriority::HIGH, std::vector<std::string> {"pylint"}));

	registry.registerParser(make_uniq<DelegatingParser<MypyParser>>(
	    "mypy_text", "Mypy Parser", ParserCategory::LINTING, "Python Mypy type checker output", ParserPriority::HIGH,
	    std::vector<std::string> {"mypy"}));

	registry.registerParser(make_uniq<DelegatingParser<Flake8Parser>>(
	    "flake8_text", "Flake8 Parser", ParserCategory::LINTING, "Python Flake8 style checker output",
	    ParserPriority::HIGH, std::vector<std::string> {"flake8"}));

	registry.registerParser(make_uniq<DelegatingParser<BlackParser>>(
	    "black_text", "Black Parser", ParserCategory::LINTING, "Python Black formatter output", ParserPriority::MEDIUM,
	    std::vector<std::string> {"black"}));

	registry.registerParser(make_uniq<DelegatingParser<YapfTextParser>>(
	    "yapf_text", "YAPF Parser", ParserCategory::LINTING, "Python YAPF formatter output", ParserPriority::VERY_HIGH,
	    std::vector<std::string> {"yapf"}));

	registry.registerParser(make_uniq<DelegatingParser<ClangTidyParser>>(
	    "clang_tidy_text", "Clang-Tidy Parser", ParserCategory::LINTING, "LLVM Clang-Tidy C++ linter output",
	    ParserPriority::HIGH, std::vector<std::string> {"clang_tidy", "clang-tidy"}));

	registry.registerParser(make_uniq<DelegatingParser<Autopep8TextParser>>(
	    "autopep8_text", "Autopep8 Parser", ParserCategory::LINTING, "Python autopep8 formatter output",
	    ParserPriority::HIGH, std::vector<std::string> {"autopep8"}));

	registry.registerParser(make_uniq<DelegatingParser<IsortParser>>(
	    "isort_text", "isort Parser", ParserCategory::LINTING, "Python isort import sorter output",
	    ParserPriority::HIGH, std::vector<std::string> {"isort"}));
}

// Auto-register this category
REGISTER_PARSER_CATEGORY(LintingTools);

} // namespace duckdb
