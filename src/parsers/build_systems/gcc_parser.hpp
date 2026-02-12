#pragma once

#include "parsers/base/parser_interface.hpp"
#include <string>
#include <vector>

namespace duckdb {

/**
 * Parser for compiler diagnostic output in the standard GCC-style format:
 *   file:line:column: severity: message
 *
 * This format is used by many compilers and tools:
 * - GCC/G++ (C/C++)
 * - Clang/Clang++ (C/C++/Objective-C)
 * - GFortran (Fortran)
 * - GNAT (Ada)
 * - Many linters and static analysis tools
 *
 * This parser is distinct from:
 * - clang_tidy_parser: handles clang-tidy with rule names like [modernize-use-nullptr]
 * - make_parser: handles make-specific output like "make: ***"
 * - cmake_parser: handles CMake configuration messages
 *
 * Supports:
 * - Error, warning, and note severity levels
 * - Optional column numbers (file:line: also works)
 * - Function context lines ("In function 'foo':")
 * - Chained diagnostics (error followed by notes)
 */
class CompilerDiagnosticParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "gcc_text";
	}
	std::string getName() const override {
		return "Compiler Diagnostic Parser";
	}
	std::string getDescription() const override {
		return "GCC-style compiler diagnostics (file:line:col: severity: message)";
	}
	int getPriority() const override {
		return ParserPriority::HIGH;
	}
	std::string getCategory() const override {
		return "build_system";
	}
	std::vector<std::string> getAliases() const override {
		return {"gcc", "g++", "clang", "clang++", "cc", "c++", "gfortran", "gnat", "compiler_diagnostic"};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    // GCC
		    CommandPattern::Literal("gcc"),
		    CommandPattern::Like("gcc %"),
		    CommandPattern::Like("gcc-%"),
		    // G++
		    CommandPattern::Literal("g++"),
		    CommandPattern::Like("g++ %"),
		    CommandPattern::Like("g++-%"),
		    // Clang
		    CommandPattern::Literal("clang"),
		    CommandPattern::Like("clang %"),
		    CommandPattern::Like("clang-%"),
		    // Clang++
		    CommandPattern::Literal("clang++"),
		    CommandPattern::Like("clang++ %"),
		    CommandPattern::Like("clang++-%"),
		    // Generic cc/c++
		    CommandPattern::Literal("cc"),
		    CommandPattern::Like("cc %"),
		    CommandPattern::Literal("c++"),
		    CommandPattern::Like("c++ %"),
		    // Fortran
		    CommandPattern::Literal("gfortran"),
		    CommandPattern::Like("gfortran %"),
		    // Ada
		    CommandPattern::Literal("gnat"),
		    CommandPattern::Like("gnat %"),
		    CommandPattern::Like("gnatmake %"),
		};
	}

private:
	bool isCompilerDiagnostic(const std::string &content) const;
};

} // namespace duckdb
