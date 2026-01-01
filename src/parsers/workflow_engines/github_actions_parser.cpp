#include "github_actions_parser.hpp"
#include "read_duck_hunt_workflow_log_function.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>

namespace duckdb {

bool GitHubActionsParser::canParse(const std::string &content) const {
	try {
		// GitHub Actions specific patterns
		return content.find("##[group]") != std::string::npos || content.find("##[endgroup]") != std::string::npos ||
		       content.find("actions/checkout@") != std::string::npos ||
		       content.find("actions/setup-") != std::string::npos || isGitHubActionsTimestamp(content);
	} catch (const std::exception &e) {
		// Error in canParse should not break the system
		return false;
	}
}

std::vector<WorkflowEvent> GitHubActionsParser::parseWorkflowLog(const std::string &content) const {
	std::vector<WorkflowEvent> events;

	try {
		if (content.empty()) {
			return events;
		}

		std::istringstream iss(content);
		std::string line;
		int event_id = 1;
		int32_t current_line_num = 0;
		std::string current_step_name = "unknown";

		while (std::getline(iss, line)) {
			current_line_num++;
			if (line.empty())
				continue;

			WorkflowEvent event;

			// Extract timestamp if present
			std::string timestamp = extractTimestamp(line);

			// Determine if this is a group marker
			bool is_group_start = line.find("##[group]") != std::string::npos;
			bool is_group_end = line.find("##[endgroup]") != std::string::npos;

			if (is_group_start) {
				// Extract step name from group marker
				size_t group_pos = line.find("##[group]");
				if (group_pos != std::string::npos) {
					current_step_name = line.substr(group_pos + 9); // Skip "##[group]"
					// Clean up the step name
					if (current_step_name.empty()) {
						current_step_name = "Step";
					}
				}
			}

			// Create base event with error handling
			try {
				event.base_event = createBaseEvent(line, "GitHub Actions Workflow", "build", current_step_name);
			} catch (const std::exception &e) {
				// If createBaseEvent fails, create minimal event
				event.base_event.event_id = event_id;
				event.base_event.tool_name = "github_actions";
				event.base_event.message = line;
				event.base_event.log_content = line;
				event.base_event.event_type = ValidationEventType::SUMMARY;
				event.base_event.status = ValidationEventStatus::INFO;
			}

			// Override/set specific fields
			event.base_event.event_id = event_id++;
			event.base_event.tool_name = "github_actions";

			if (!timestamp.empty()) {
				event.base_event.started_at = timestamp;
			}

			// Determine severity based on content
			if (line.find("ERROR") != std::string::npos || line.find("FAIL") != std::string::npos) {
				event.base_event.status = ValidationEventStatus::ERROR;
				event.base_event.severity = "error";
			} else if (line.find("WARN") != std::string::npos) {
				event.base_event.status = ValidationEventStatus::WARNING;
				event.base_event.severity = "warning";
			} else if (line.find("PASS") != std::string::npos || line.find("✓") != std::string::npos) {
				event.base_event.status = ValidationEventStatus::PASS;
				event.base_event.severity = "info";
			} else {
				event.base_event.status = ValidationEventStatus::INFO;
				event.base_event.severity = "info";
			}

			// Set workflow-specific fields
			event.workflow_type = "github_actions";
			if (is_group_start || is_group_end) {
				event.hierarchy_level = 2; // Step level
			} else {
				event.hierarchy_level = 3; // Line level within step
			}
			event.parent_id = "job_1";
			event.base_event.log_line_start = current_line_num;
			event.base_event.log_line_end = current_line_num;

			events.push_back(event);
		}

		// If no events were created but content exists, create a summary event
		if (events.empty() && !content.empty()) {
			WorkflowEvent summary_event;
			summary_event.base_event.event_id = 1;
			summary_event.base_event.tool_name = "github_actions";
			summary_event.base_event.message = "Workflow log processed";
			summary_event.base_event.log_content = content.substr(0, std::min(size_t(500), content.length()));
			summary_event.base_event.event_type = ValidationEventType::SUMMARY;
			summary_event.base_event.status = ValidationEventStatus::INFO;
			summary_event.base_event.severity = "info";

			summary_event.workflow_type = "github_actions";
			summary_event.hierarchy_level = 1;
			summary_event.parent_id = "workflow_1";

			events.push_back(summary_event);
		}

	} catch (const std::exception &e) {
		// If parsing fails completely, create an error event
		WorkflowEvent error_event;
		error_event.base_event.event_id = 1;
		error_event.base_event.tool_name = "github_actions";
		error_event.base_event.message = std::string("Parser error: ") + e.what();
		error_event.base_event.log_content = content.substr(0, std::min(size_t(200), content.length()));
		error_event.base_event.event_type = ValidationEventType::SUMMARY;
		error_event.base_event.status = ValidationEventStatus::ERROR;
		error_event.base_event.severity = "error";

		error_event.workflow_type = "github_actions";
		error_event.hierarchy_level = 1;
		error_event.parent_id = "error";

		events.push_back(error_event);
	}

	return events;
}

bool GitHubActionsParser::isGitHubActionsTimestamp(const std::string &line) const {
	// GitHub Actions timestamp format: 2023-10-15T14:30:15.1234567Z
	std::regex timestamp_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z)");
	return std::regex_search(line, timestamp_pattern);
}

bool GitHubActionsParser::isGroupStart(const std::string &line) const {
	return line.find("##[group]") != std::string::npos;
}

bool GitHubActionsParser::isGroupEnd(const std::string &line) const {
	return line.find("##[endgroup]") != std::string::npos;
}

bool GitHubActionsParser::isActionStep(const std::string &line) const {
	return line.find("actions/") != std::string::npos || line.find("Run ") != std::string::npos ||
	       line.find("shell: ") != std::string::npos;
}

std::string GitHubActionsParser::extractWorkflowName(const std::string &content) const {
	// Try to extract from job/step context
	std::istringstream iss(content);
	std::string line;

	while (std::getline(iss, line)) {
		if (line.find("##[group]Run ") != std::string::npos) {
			size_t start = line.find("Run ") + 4;
			size_t end = line.find_first_of("\r\n", start);
			if (end == std::string::npos)
				end = line.length();
			return line.substr(start, end - start);
		}
	}

	// Default workflow name
	return "GitHub Actions Workflow";
}

std::string GitHubActionsParser::extractRunId(const std::string &content) const {
	// GitHub Actions doesn't usually include run ID in logs
	// Generate a simple hash-based ID
	std::hash<std::string> hasher;
	return std::to_string(hasher(content.substr(0, 100)));
}

std::string GitHubActionsParser::extractJobName(const std::string &line) const {
	// Extract job name from context - for now use a default
	return "build"; // Most common GitHub Actions job name
}

std::string GitHubActionsParser::extractStepName(const std::string &line) const {
	if (isGroupStart(line)) {
		size_t start = line.find("##[group]") + 9;
		size_t end = line.find_first_of("\r\n", start);
		if (end == std::string::npos)
			end = line.length();
		return line.substr(start, end - start);
	}

	if (line.find("actions/") != std::string::npos) {
		size_t start = line.find("actions/");
		size_t end = line.find("@", start);
		if (end != std::string::npos) {
			return line.substr(start, end - start);
		}
	}

	return "";
}

std::string GitHubActionsParser::extractTimestamp(const std::string &line) const {
	std::regex timestamp_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z)");
	std::smatch match;
	if (std::regex_search(line, match, timestamp_pattern)) {
		return match.str();
	}
	return "";
}

std::string GitHubActionsParser::extractStatus(const std::string &line) const {
	std::string lower_line = StringUtil::Lower(line);

	if (lower_line.find("error") != std::string::npos || lower_line.find("failed") != std::string::npos ||
	    lower_line.find("fail") != std::string::npos) {
		return "failure";
	}

	if (lower_line.find("warning") != std::string::npos || lower_line.find("warn") != std::string::npos) {
		return "warning";
	}

	if (lower_line.find("success") != std::string::npos || lower_line.find("completed") != std::string::npos) {
		return "success";
	}

	return "running";
}

std::vector<GitHubActionsParser::GitHubJob> GitHubActionsParser::parseJobs(const std::string &content) const {
	std::vector<GitHubJob> jobs;
	GitHubJob current_job;
	current_job.job_name = "build";
	current_job.job_id = "job_1";
	current_job.status = "running";

	std::istringstream iss(content);
	std::string line;
	std::vector<std::string> current_step_lines;
	bool in_step = false;

	while (std::getline(iss, line)) {
		if (isGroupStart(line)) {
			// Starting a new step
			if (in_step && !current_step_lines.empty()) {
				// Finish previous step
				current_job.steps.push_back(parseStep(current_step_lines));
				current_step_lines.clear();
			}
			in_step = true;
			current_step_lines.push_back(line);
		} else if (isGroupEnd(line)) {
			// Ending current step
			if (in_step) {
				current_step_lines.push_back(line);
				current_job.steps.push_back(parseStep(current_step_lines));
				current_step_lines.clear();
				in_step = false;
			}
		} else if (in_step) {
			current_step_lines.push_back(line);
		} else {
			// Lines outside of steps - could be job-level info
			current_step_lines.push_back(line);
		}
	}

	// Handle remaining step if any
	if (in_step && !current_step_lines.empty()) {
		current_job.steps.push_back(parseStep(current_step_lines));
	}

	// If no steps were found, create a default step
	if (current_job.steps.empty()) {
		GitHubStep default_step;
		default_step.step_name = "Workflow Execution";
		default_step.step_id = "step_1";
		default_step.status = "success";
		default_step.output_lines = {content.substr(0, std::min(size_t(500), content.length()))};
		current_job.steps.push_back(default_step);
	}

	jobs.push_back(current_job);
	return jobs;
}

GitHubActionsParser::GitHubStep GitHubActionsParser::parseStep(const std::vector<std::string> &step_lines) const {
	GitHubStep step;

	if (step_lines.empty()) {
		step.step_name = "Unknown Step";
		step.step_id = "unknown";
		step.status = "unknown";
		return step;
	}

	// Extract step name from first line (group start)
	step.step_name = extractStepName(step_lines[0]);
	if (step.step_name.empty()) {
		step.step_name = "Unnamed Step";
	}

	// Generate step ID
	std::hash<std::string> hasher;
	step.step_id = "step_" + std::to_string(hasher(step.step_name) % 10000);

	// Extract timestamps and status
	for (const auto &line : step_lines) {
		std::string timestamp = extractTimestamp(line);
		if (!timestamp.empty()) {
			if (step.started_at.empty()) {
				step.started_at = timestamp;
			}
			step.completed_at = timestamp; // Last timestamp is completion
		}
	}

	// Determine overall step status
	step.status = "success"; // Default
	for (const auto &line : step_lines) {
		std::string line_status = extractStatus(line);
		if (line_status == "failure") {
			step.status = "failure";
			break;
		} else if (line_status == "warning" && step.status != "failure") {
			step.status = "warning";
		}
	}

	step.output_lines = step_lines;
	return step;
}

std::vector<WorkflowEvent> GitHubActionsParser::convertToEvents(const std::vector<GitHubJob> &jobs,
                                                                const std::string &workflow_name,
                                                                const std::string &run_id) const {
	std::vector<WorkflowEvent> events;

	for (const auto &job : jobs) {
		for (const auto &step : job.steps) {
			for (const auto &output_line : step.output_lines) {
				WorkflowEvent event;

				// Create base validation event
				ValidationEvent base_event = createBaseEvent(output_line, workflow_name, job.job_name, step.step_name);

				// Set the base_event
				event.base_event = base_event;

				// Override specific fields in base_event (Schema V2)
				event.base_event.status = ValidationEventStatus::INFO;
				event.base_event.severity = determineSeverity(step.status, output_line);
				event.base_event.scope = workflow_name;
				event.base_event.group = job.job_name;
				event.base_event.unit = step.step_name;
				event.base_event.scope_id = run_id;
				event.base_event.group_id = job.job_id;
				event.base_event.unit_id = step.step_id;
				event.base_event.scope_status = "running";
				event.base_event.group_status = job.status;
				event.base_event.unit_status = step.status;
				event.base_event.started_at = step.started_at;

				// Set workflow-specific fields
				event.workflow_type = "github_actions";
				event.hierarchy_level = 3; // Step level
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
