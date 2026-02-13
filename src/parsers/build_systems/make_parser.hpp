#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Make build system output.
 *
 * Handles make-specific patterns:
 * - Recipe failures: "make: *** [target] Error N"
 * - Submake errors: "make[N]: *** [target] Error N"
 * - Directory tracking: "make[N]: Entering/Leaving directory"
 *
 * Does NOT parse GCC/Clang diagnostics (file:line: error:) - use gcc_text parser for those.
 * For full make build output, use auto-detection or combine with gcc_text parser.
 */
class MakeParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "make_error";
	}
	std::string getName() const override {
		return "make";
	}
	std::string getDescription() const override {
		return "Make build system output (recipe failures, directory tracking)";
	}
	int getPriority() const override {
		return ParserPriority::MEDIUM;
	}
	std::string getCategory() const override {
		return "build_system";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("make"),
		    CommandPattern::Like("make %"),
		    CommandPattern::Literal("gmake"),
		    CommandPattern::Like("gmake %"),
		};
	}
};

} // namespace duckdb
