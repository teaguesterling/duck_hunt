#pragma once

#include "parsers/base/parser_interface.hpp"
#include "include/validation_event_types.hpp"
#include <vector>
#include <string>

namespace duckdb {

class GitHubCliParser : public IParser {
public:
	GitHubCliParser() = default;
	~GitHubCliParser() = default;

	std::string getName() const override {
		return "GitHub CLI Parser";
	}
	std::string getCategory() const override {
		return "CI/CD Systems";
	}
	std::string getFormatName() const override {
		return "github_cli";
	}
	int getPriority() const override {
		return 85;
	}

	bool canParse(const std::string &content) const override;
	std::vector<ValidationEvent> parse(const std::string &content) const override;

private:
	bool isGitHubRunsList(const std::string &content) const;
	bool isGitHubRunView(const std::string &content) const;
	bool isGitHubWorkflowLog(const std::string &content) const;

	std::vector<ValidationEvent> parseRunsList(const std::string &content) const;
	std::vector<ValidationEvent> parseRunView(const std::string &content) const;
	std::vector<ValidationEvent> parseWorkflowLog(const std::string &content) const;
};

} // namespace duckdb
