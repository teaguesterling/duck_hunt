#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for GitHub Actions workflow log output.
 * Detects step markers, error annotations, and workflow patterns.
 */
class GitHubActionsTextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "github_actions_text";
	}
	std::string getName() const override {
		return "github_actions";
	}
	std::string getDescription() const override {
		return "GitHub Actions workflow log output";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "ci_system";
	}
};

} // namespace duckdb
