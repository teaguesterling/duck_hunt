#pragma once

#include "parsers/base/parser_interface.hpp"

namespace duckdb {

/**
 * Parser for LCOV code coverage format.
 *
 * LCOV is the standard format produced by gcov/lcov for C/C++ coverage,
 * and is also used by many other coverage tools across languages.
 *
 * Format specification:
 *   TN:<test name>           - Test name (optional)
 *   SF:<source file>         - Source file path
 *   FN:<line>,<name>         - Function at line
 *   FNDA:<count>,<name>      - Function hit count
 *   FNF:<count>              - Functions found
 *   FNH:<count>              - Functions hit
 *   DA:<line>,<count>        - Line data (hit count)
 *   LF:<count>               - Lines found
 *   LH:<count>               - Lines hit
 *   BRDA:<line>,<block>,<branch>,<count>  - Branch data
 *   BRF:<count>              - Branches found
 *   BRH:<count>              - Branches hit
 *   end_of_record            - End of source file block
 *
 * Example:
 *   SF:src/main.cpp
 *   FN:10,main
 *   FNDA:1,main
 *   DA:10,1
 *   DA:11,1
 *   DA:12,0
 *   LF:3
 *   LH:2
 *   end_of_record
 */
class LcovParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "lcov";
	}
	std::string getName() const override {
		return "lcov";
	}
	int getPriority() const override {
		return 75;
	} // Medium-high priority for coverage files
	std::string getCategory() const override {
		return "coverage";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("lcov"),
		    CommandPattern::Like("lcov %"),
		    CommandPattern::Literal("geninfo"),
		    CommandPattern::Like("geninfo %"),
		};
	}
};

} // namespace duckdb
