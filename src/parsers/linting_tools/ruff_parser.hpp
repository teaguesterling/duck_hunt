#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Ruff (fast Python linter) output.
 * Ruff uses a Rust-style diagnostic format with --> file:line:col markers.
 */
class RuffParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "ruff_text";
	}
	std::string getName() const override {
		return "ruff";
	}
	std::string getDescription() const override {
		return "Ruff Python linter output";
	}
	int getPriority() const override {
		return 100; // VERY_HIGH - ruff has distinctive --> format, check before flake8
	}
	std::string getCategory() const override {
		return "linting_tool";
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("ruff"),
		    CommandPattern::Like("ruff %"),
		    CommandPattern::Like("ruff check%"),
		    CommandPattern::Like("python -m ruff%"),
		};
	}
};

} // namespace duckdb
