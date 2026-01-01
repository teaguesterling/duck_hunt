#pragma once

#include "workflow_engine_interface.hpp"
#include <regex>

namespace duckdb {

class JenkinsParser : public WorkflowEngineParser {
public:
	bool canParse(const std::string &content) const override;
	std::string getFormatName() const override {
		return "jenkins";
	}
	std::vector<WorkflowEvent> parseWorkflowLog(const std::string &content) const override;
	int getPriority() const override {
		return 130;
	} // High priority for Jenkins
	std::string getName() const override {
		return "JenkinsParser";
	}

private:
	struct JenkinsStep {
		std::string step_name;
		std::string step_id;
		std::string status;
		std::string started_at;
		std::string completed_at;
		std::vector<std::string> output_lines;
	};

	struct JenkinsBuild {
		std::string build_name;
		std::string build_id;
		std::string build_number;
		std::string status;
		std::string workspace;
		std::vector<JenkinsStep> steps;
	};

	// Parse Jenkins specific patterns
	bool isJenkinsConsole(const std::string &line) const;
	bool isBuildStart(const std::string &line) const;
	bool isBuildEnd(const std::string &line) const;
	bool isStepMarker(const std::string &line) const;
	bool isWorkspaceInfo(const std::string &line) const;

	// Extract workflow metadata
	std::string extractWorkflowName(const std::string &content) const;
	std::string extractBuildId(const std::string &content) const;
	std::string extractBuildNumber(const std::string &content) const;
	std::string extractBuildName(const std::string &line) const;
	std::string extractStepName(const std::string &line) const;
	std::string extractWorkspace(const std::string &content) const;
	std::string extractTimestamp(const std::string &line) const;
	std::string extractStatus(const std::string &line) const;

	// Parse hierarchical structure
	std::vector<JenkinsBuild> parseBuilds(const std::string &content) const;
	JenkinsStep parseStep(const std::vector<std::string> &step_lines, const std::string &step_name) const;

	// Convert to ValidationEvents
	std::vector<WorkflowEvent> convertToEvents(const std::vector<JenkinsBuild> &builds,
	                                           const std::string &workflow_name) const;
};

} // namespace duckdb
