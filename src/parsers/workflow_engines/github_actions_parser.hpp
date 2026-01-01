#pragma once

#include "workflow_engine_interface.hpp"
#include <regex>

namespace duckdb {

class GitHubActionsParser : public WorkflowEngineParser {
public:
	bool canParse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "github_actions";
	}
	std::vector<WorkflowEvent> parseWorkflowLog(const std::string &content) const override;
	int getPriority() const override {
		return 150;
	} // High priority for GitHub Actions
	std::string getName() const override {
		return "GitHubActionsParser";
	}

private:
	struct GitHubStep {
		std::string step_name;
		std::string step_id;
		std::string started_at;
		std::string completed_at;
		std::string status;
		std::vector<std::string> output_lines;
	};

	struct GitHubJob {
		std::string job_name;
		std::string job_id;
		std::string status;
		std::vector<GitHubStep> steps;
	};

	// Parse GitHub Actions specific patterns
	bool isGitHubActionsTimestamp(const std::string &line) const;
	bool isGroupStart(const std::string &line) const;
	bool isGroupEnd(const std::string &line) const;
	bool isActionStep(const std::string &line) const;

	// Extract workflow metadata
	std::string extractWorkflowName(const std::string &content) const;
	std::string extractRunId(const std::string &content) const;
	std::string extractJobName(const std::string &line) const;
	std::string extractStepName(const std::string &line) const;
	std::string extractTimestamp(const std::string &line) const;
	std::string extractStatus(const std::string &line) const;

	// Parse hierarchical structure
	std::vector<GitHubJob> parseJobs(const std::string &content) const;
	GitHubStep parseStep(const std::vector<std::string> &step_lines) const;

	// Convert to ValidationEvents
	std::vector<WorkflowEvent> convertToEvents(const std::vector<GitHubJob> &jobs, const std::string &workflow_name,
	                                           const std::string &run_id) const;
};

} // namespace duckdb
