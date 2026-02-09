#pragma once

#include "parsers/base/parser_interface.hpp"
#include <regex>

namespace duckdb {

/**
 * Parser for GitLab CI pipeline log output.
 * Detects job sections, script execution, and error patterns.
 */
class GitLabCITextParser : public IParser {
public:
	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "gitlab_ci_text";
	}
	std::string getName() const override {
		return "gitlab_ci";
	}
	std::string getDescription() const override {
		return "GitLab CI pipeline log output";
	}
	int getPriority() const override {
		return 80;
	}
	std::string getCategory() const override {
		return "ci_system";
	}
};

} // namespace duckdb
