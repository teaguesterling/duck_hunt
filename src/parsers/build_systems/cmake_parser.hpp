#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for CMake build output.
 *
 * Handles cmake-specific patterns:
 * - Configuration errors: "CMake Error at file:line ..."
 * - Configuration warnings: "CMake Warning at file:line ..."
 * - Deprecation/developer warnings
 * - Configuration summary: "-- Configuring incomplete"
 * - gmake failures: "gmake[N]: *** [target] Error N"
 *
 * Does NOT parse GCC/Clang diagnostics (file:line: error:) - use gcc_text parser for those.
 * For full cmake build output, use auto-detection or combine with gcc_text parser.
 */
class CMakeParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "cmake_build";
	}
	std::string getName() const override {
		return "CMake Build Parser";
	}
	int getPriority() const override {
		return ParserPriority::MEDIUM;
	}
	std::string getCategory() const override {
		return "build_system";
	}
	std::string getDescription() const override {
		return "CMake configuration output (errors, warnings, gmake failures)";
	}
	std::vector<std::string> getAliases() const override {
		return {"cmake"};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("cmake"),
		    CommandPattern::Like("cmake %"),
		    CommandPattern::Like("cmake --build%"),
		};
	}
};

} // namespace duckdb
