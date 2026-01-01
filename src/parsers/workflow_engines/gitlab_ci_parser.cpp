#include "gitlab_ci_parser.hpp"
#include "read_duck_hunt_workflow_log_function.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>

namespace duckdb {

bool GitLabCIParser::canParse(const std::string &content) const {
	// GitLab CI specific patterns
	return content.find("Running with gitlab-runner") != std::string::npos ||
	       content.find("Using Docker executor") != std::string::npos ||
	       content.find("Preparing the \"docker\" executor") != std::string::npos ||
	       content.find("Getting source from Git repository") != std::string::npos ||
	       content.find("Pipeline #") != std::string::npos || content.find("Job succeeded") != std::string::npos;
}

std::vector<WorkflowEvent> GitLabCIParser::parseWorkflowLog(const std::string &content) const {
	std::vector<WorkflowEvent> events;

	// Extract workflow metadata
	std::string workflow_name = extractWorkflowName(content);
	std::string pipeline_id = extractPipelineId(content);

	// Parse jobs and stages
	std::vector<GitLabJob> jobs = parseJobs(content);

	// Convert to events
	return convertToEvents(jobs, workflow_name, pipeline_id);
}

bool GitLabCIParser::isGitLabRunner(const std::string &line) const {
	return line.find("Running with gitlab-runner") != std::string::npos ||
	       line.find("on gitlab-runner") != std::string::npos;
}

bool GitLabCIParser::isDockerExecutor(const std::string &line) const {
	return line.find("Using Docker executor") != std::string::npos ||
	       line.find("Preparing the \"docker\" executor") != std::string::npos;
}

bool GitLabCIParser::isJobStart(const std::string &line) const {
	return line.find("Running on runner-") != std::string::npos ||
	       line.find("Preparing environment") != std::string::npos;
}

bool GitLabCIParser::isJobEnd(const std::string &line) const {
	return line.find("Job succeeded") != std::string::npos || line.find("Job failed") != std::string::npos ||
	       line.find("Pipeline #") != std::string::npos;
}

bool GitLabCIParser::isStageMarker(const std::string &line) const {
	return line.find("Executing \"") != std::string::npos ||
	       line.find("Getting source from Git repository") != std::string::npos ||
	       line.find("Preparing environment") != std::string::npos;
}

std::string GitLabCIParser::extractWorkflowName(const std::string &content) const {
	// Try to extract from pipeline context
	std::istringstream iss(content);
	std::string line;

	while (std::getline(iss, line)) {
		if (line.find("Pipeline #") != std::string::npos) {
			return "GitLab CI Pipeline";
		}
	}

	// Default workflow name
	return "GitLab CI Pipeline";
}

std::string GitLabCIParser::extractPipelineId(const std::string &content) const {
	std::regex pipeline_pattern(R"(Pipeline #(\d+))");
	std::smatch match;
	if (std::regex_search(content, match, pipeline_pattern)) {
		return match[1].str();
	}

	// Generate a simple hash-based ID
	std::hash<std::string> hasher;
	return std::to_string(hasher(content.substr(0, 100)) % 100000);
}

std::string GitLabCIParser::extractJobName(const std::string &line) const {
	// Extract job name from runner context
	if (line.find("runner-") != std::string::npos) {
		// Most GitLab CI jobs are named after their stage
		return "test"; // Common default
	}
	return "gitlab-job";
}

std::string GitLabCIParser::extractStageName(const std::string &line) const {
	if (line.find("Executing \"") != std::string::npos) {
		size_t start = line.find("Executing \"") + 11;
		size_t end = line.find("\" stage", start);
		if (end != std::string::npos) {
			return line.substr(start, end - start);
		}
	}

	if (line.find("Getting source from Git repository") != std::string::npos) {
		return "git_clone";
	}

	if (line.find("Preparing environment") != std::string::npos) {
		return "setup";
	}

	return "script";
}

std::string GitLabCIParser::extractExecutor(const std::string &content) const {
	if (content.find("Using Docker executor") != std::string::npos) {
		return "docker";
	}
	if (content.find("shell executor") != std::string::npos) {
		return "shell";
	}
	return "unknown";
}

std::string GitLabCIParser::extractRunnerInfo(const std::string &line) const {
	std::regex runner_pattern(R"(on ([\w\-\.]+))");
	std::smatch match;
	if (std::regex_search(line, match, runner_pattern)) {
		return match[1].str();
	}
	return "";
}

std::string GitLabCIParser::extractStatus(const std::string &line) const {
	std::string lower_line = StringUtil::Lower(line);

	if (lower_line.find("job succeeded") != std::string::npos ||
	    (lower_line.find("pipeline") != std::string::npos && lower_line.find("passed") != std::string::npos)) {
		return "success";
	}

	if (lower_line.find("job failed") != std::string::npos || lower_line.find("error") != std::string::npos ||
	    lower_line.find("failed") != std::string::npos) {
		return "failure";
	}

	if (lower_line.find("warning") != std::string::npos || lower_line.find("deprecated") != std::string::npos) {
		return "warning";
	}

	return "running";
}

std::vector<GitLabCIParser::GitLabJob> GitLabCIParser::parseJobs(const std::string &content) const {
	std::vector<GitLabJob> jobs;
	GitLabJob current_job;
	current_job.job_name = "test";
	current_job.job_id = "job_1";
	current_job.status = "running";
	current_job.executor = extractExecutor(content);

	std::istringstream iss(content);
	std::string line;
	std::vector<std::string> current_stage_lines;
	std::string current_stage_name;

	while (std::getline(iss, line)) {
		if (isStageMarker(line)) {
			// Starting a new stage
			if (!current_stage_lines.empty() && !current_stage_name.empty()) {
				// Finish previous stage
				current_job.stages.push_back(parseStage(current_stage_lines, current_stage_name));
				current_stage_lines.clear();
			}
			current_stage_name = extractStageName(line);
			current_stage_lines.push_back(line);
		} else {
			current_stage_lines.push_back(line);

			// Check for job status updates
			if (isJobEnd(line)) {
				current_job.status = extractStatus(line);
			}
		}
	}

	// Handle remaining stage if any
	if (!current_stage_lines.empty() && !current_stage_name.empty()) {
		current_job.stages.push_back(parseStage(current_stage_lines, current_stage_name));
	}

	// If no stages were found, create a default stage
	if (current_job.stages.empty()) {
		GitLabStage default_stage;
		default_stage.stage_name = "Pipeline Execution";
		default_stage.stage_id = "stage_1";
		default_stage.status = "success";
		default_stage.output_lines = {content.substr(0, std::min(size_t(500), content.length()))};
		current_job.stages.push_back(default_stage);
	}

	jobs.push_back(current_job);
	return jobs;
}

GitLabCIParser::GitLabStage GitLabCIParser::parseStage(const std::vector<std::string> &stage_lines,
                                                       const std::string &stage_name) const {
	GitLabStage stage;
	stage.stage_name = stage_name;

	// Generate stage ID
	std::hash<std::string> hasher;
	stage.stage_id = "stage_" + std::to_string(hasher(stage_name) % 10000);

	// Extract timestamps and status
	for (const auto &line : stage_lines) {
		// GitLab CI doesn't usually have detailed timestamps in logs
		// Look for time markers like "00:01", "00:02" etc.
		std::regex time_pattern(R"(\d{2}:\d{2})");
		std::smatch match;
		if (std::regex_search(line, match, time_pattern)) {
			if (stage.started_at.empty()) {
				stage.started_at = match.str();
			}
			stage.completed_at = match.str(); // Last time is completion
		}
	}

	// Determine overall stage status
	stage.status = "success"; // Default
	for (const auto &line : stage_lines) {
		std::string line_status = extractStatus(line);
		if (line_status == "failure") {
			stage.status = "failure";
			break;
		} else if (line_status == "warning" && stage.status != "failure") {
			stage.status = "warning";
		}
	}

	stage.output_lines = stage_lines;
	return stage;
}

std::vector<WorkflowEvent> GitLabCIParser::convertToEvents(const std::vector<GitLabJob> &jobs,
                                                           const std::string &workflow_name,
                                                           const std::string &pipeline_id) const {
	std::vector<WorkflowEvent> events;

	for (const auto &job : jobs) {
		for (const auto &stage : job.stages) {
			for (const auto &output_line : stage.output_lines) {
				WorkflowEvent event;

				// Create base validation event
				ValidationEvent base_event =
				    createBaseEvent(output_line, workflow_name, job.job_name, stage.stage_name);

				// Set the base_event
				event.base_event = base_event;

				// Override specific fields in base_event (Schema V2)
				event.base_event.status = ValidationEventStatus::INFO;
				event.base_event.severity = determineSeverity(stage.status, output_line);
				event.base_event.scope = workflow_name;
				event.base_event.group = job.job_name;
				event.base_event.unit = stage.stage_name;
				event.base_event.scope_id = pipeline_id;
				event.base_event.group_id = job.job_id;
				event.base_event.unit_id = stage.stage_id;
				event.base_event.scope_status = "running";
				event.base_event.group_status = job.status;
				event.base_event.unit_status = stage.status;
				event.base_event.started_at = stage.started_at;
				event.workflow_type = "gitlab_ci";
				event.hierarchy_level = 3; // Stage level (equivalent to step)
				event.parent_id = job.job_id;

				events.push_back(event);
			}
		}
	}

	return events;
}

// NOTE: Parser registration is handled manually in ReadDuckHuntWorkflowLogInitGlobal
// to avoid static initialization order issues across platforms.
// Do not use REGISTER_WORKFLOW_PARSER macro here.

} // namespace duckdb
