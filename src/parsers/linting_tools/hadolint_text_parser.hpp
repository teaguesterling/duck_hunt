#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for Hadolint text output.
 * Handles linting issues from the Hadolint Dockerfile linter.
 *
 * Example format:
 * Dockerfile:1 DL3006 warning: Always tag the version of an image explicitly
 * Dockerfile:3 DL3008 warning: Pin versions in apt get install
 */
class HadolintTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

	std::string getFormatName() const override {
		return "hadolint_text";
	}
	std::string getName() const override {
		return "Hadolint Text Parser";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "linting_tool";
	}
	std::string getDescription() const override {
		return "Hadolint Dockerfile linter text output";
	}
	std::vector<std::string> getAliases() const override {
		return {"hadolint"};
	}
	std::vector<CommandPattern> getCommandPatterns() const override {
		return {
		    CommandPattern::Literal("hadolint"),
		    CommandPattern::Like("hadolint %"),
		    CommandPattern::Like("hadolint Dockerfile%"),
		    CommandPattern::Regexp("hadolint\\s+(?!.*(-f|--format)[= ]?json)"),
		};
	}
	std::vector<std::string> getGroups() const override {
		return {"docker", "lint", "infrastructure"};
	}
};

} // namespace duckdb
