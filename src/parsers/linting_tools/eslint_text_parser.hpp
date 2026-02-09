#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for ESLint text output (default "stylish" formatter).
 * Handles linting issues from the ESLint JavaScript/TypeScript linter.
 *
 * Example format:
 * /path/to/file.js
 *   1:10  error    Unexpected var  no-var
 *   2:5   warning  Unexpected console  no-console
 */
class EslintTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "eslint_text";
	}
	std::string getName() const override {
		return "ESLint Text Parser";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "linting_tool";
	}
	std::string getDescription() const override {
		return "ESLint JavaScript/TypeScript linter text output";
	}
	std::vector<std::string> getAliases() const override {
		return {"eslint"};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("eslint"),
		    CommandPattern::Like("eslint %"),
		    CommandPattern::Like("npx eslint %"),
		    CommandPattern::Like("yarn eslint %"),
		    CommandPattern::Like("pnpm eslint %"),
		    CommandPattern::Regexp("eslint\\s+(?!.*(-f|--format)\\s*json)"),
		};
	}
	std::vector<std::string> getGroups() const override {
		return {"javascript", "lint"};
	}
};

} // namespace duckdb
