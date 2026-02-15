#include "github_actions_parser.hpp"
#include "read_duck_hunt_workflow_log_function.hpp"
#include "core/parser_registry.hpp"
#include "duckdb/common/string_util.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for GitHub Actions parsing (compiled once, reused)
namespace {
static const std::regex RE_TIMESTAMP_PATTERN(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z)");
} // anonymous namespace

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

		// Use structured parsing with delegation support
		std::string workflow_name = extractWorkflowName(content);
		std::string run_id = extractRunId(content);
		std::vector<GitHubJob> jobs = parseJobs(content);

		// Convert to events (uses delegation when available)
		events = convertToEvents(jobs, workflow_name, run_id);

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
	return std::regex_search(line, RE_TIMESTAMP_PATTERN);
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
	std::smatch match;
	if (std::regex_search(line, match, RE_TIMESTAMP_PATTERN)) {
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

std::string GitHubActionsParser::extractCommand(const std::string &step_name) const {
	// Extract command from step names like "Run make release" or "Run pytest tests/"
	if (step_name.find("Run ") == 0) {
		std::string command = step_name.substr(4); // Skip "Run "
		// Trim leading/trailing whitespace
		size_t start = command.find_first_not_of(" \t\r\n");
		size_t end = command.find_last_not_of(" \t\r\n");
		if (start != std::string::npos && end != std::string::npos) {
			return command.substr(start, end - start + 1);
		}
		return command;
	}
	return "";
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

	// Handle job-level lines that came after the last step (e.g., ##[error], FAILED:)
	if (!in_step && !current_step_lines.empty()) {
		GitHubStep job_level_step;
		job_level_step.step_name = "Job Level";
		job_level_step.step_id = "job_level";
		job_level_step.status = "info";
		job_level_step.output_lines = current_step_lines;

		// Check status from content
		for (const auto &line : current_step_lines) {
			if (line.find("##[error]") != std::string::npos || line.find("FAIL") != std::string::npos) {
				job_level_step.status = "failure";
				break;
			} else if (line.find("##[warning]") != std::string::npos) {
				if (job_level_step.status != "failure") {
					job_level_step.status = "warning";
				}
			}
		}

		current_job.steps.push_back(job_level_step);
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

	// Try to delegate parsing to a specialized parser based on command
	step.detected_command = extractCommand(step.step_name);
	if (!step.detected_command.empty()) {
		auto &registry = ParserRegistry::getInstance();
		IParser *delegated_parser = registry.findParserByCommand(step.detected_command);

		if (delegated_parser) {
			step.delegated_format = delegated_parser->getFormatName();

			// Build content string from output lines (skip group markers)
			std::string step_content;
			for (size_t i = 1; i < step_lines.size(); i++) {
				const auto &line = step_lines[i];
				// Skip group end marker
				if (line.find("##[endgroup]") != std::string::npos) {
					continue;
				}
				// Strip timestamp prefix if present (2023-10-15T14:30:15.1234567Z )
				size_t content_start = 0;
				if (line.size() > 28 && line[4] == '-' && line[10] == 'T' && line[27] == 'Z') {
					content_start = 29; // Skip "YYYY-MM-DDTHH:MM:SS.fffffffZ "
				}
				if (content_start < line.size()) {
					step_content += line.substr(content_start) + "\n";
				}
			}

			// Parse with delegated parser
			if (!step_content.empty()) {
				try {
					step.delegated_events = delegated_parser->parse(step_content);
				} catch (const std::exception &) {
					// Delegation failed, will fall back to workflow-level parsing
					step.delegated_events.clear();
				}
			}
		}
	}

	return step;
}

std::vector<WorkflowEvent> GitHubActionsParser::convertToEvents(const std::vector<GitHubJob> &jobs,
                                                                const std::string &workflow_name,
                                                                const std::string &run_id) const {
	std::vector<WorkflowEvent> events;

	for (const auto &job : jobs) {
		for (const auto &step : job.steps) {
			// If we have delegated events, use those instead of raw output lines
			if (!step.delegated_events.empty()) {
				for (const auto &delegated_event : step.delegated_events) {
					WorkflowEvent event;
					event.base_event = delegated_event;

					// Enrich with workflow context
					event.base_event.scope = workflow_name;
					event.base_event.group = job.job_name;
					event.base_event.unit = step.step_name;
					event.base_event.scope_id = run_id;
					event.base_event.group_id = job.job_id;
					event.base_event.unit_id = step.step_id;
					event.base_event.scope_status = "running";
					event.base_event.group_status = job.status;
					event.base_event.unit_status = step.status;
					if (event.base_event.started_at.empty()) {
						event.base_event.started_at = step.started_at;
					}

					// Mark as delegated
					event.workflow_type = "github_actions";
					event.hierarchy_level = 4; // Delegated tool event level
					event.parent_id = step.step_id;

					// Add delegation info to structured_data
					if (!step.delegated_format.empty()) {
						event.base_event.structured_data = step.delegated_format;
					}

					events.push_back(event);
				}
			} else {
				// No delegation - emit only meaningful workflow events
				for (const auto &output_line : step.output_lines) {
					// Filter to meaningful lines only
					bool is_group_start = output_line.find("##[group]") != std::string::npos;
					bool is_group_end = output_line.find("##[endgroup]") != std::string::npos;
					bool is_error_cmd = output_line.find("##[error]") != std::string::npos;
					bool is_warning_cmd = output_line.find("##[warning]") != std::string::npos;
					bool is_notice_cmd = output_line.find("##[notice]") != std::string::npos;
					bool has_error_keyword =
					    output_line.find("ERROR") != std::string::npos || output_line.find("FAIL") != std::string::npos;
					bool has_warning_keyword = output_line.find("WARN") != std::string::npos;
					bool has_pass_keyword =
					    output_line.find("PASS") != std::string::npos || output_line.find("✓") != std::string::npos;
					bool is_action_ref = output_line.find("actions/") != std::string::npos;
					bool is_job_info = output_line.find("Complete job name:") != std::string::npos;

					bool is_meaningful = is_group_start || is_group_end || is_error_cmd || is_warning_cmd ||
					                     is_notice_cmd || has_error_keyword || has_warning_keyword ||
					                     has_pass_keyword || is_action_ref || is_job_info;

					if (!is_meaningful) {
						continue;
					}

					WorkflowEvent event;

					// Create base validation event
					ValidationEvent base_event =
					    createBaseEvent(output_line, workflow_name, job.job_name, step.step_name);

					// Set the base_event
					event.base_event = base_event;

					// Determine severity from workflow commands and keywords
					std::string severity = "info";
					ValidationEventStatus status = ValidationEventStatus::INFO;
					if (is_error_cmd || has_error_keyword) {
						severity = "error";
						status = ValidationEventStatus::ERROR;
					} else if (is_warning_cmd || has_warning_keyword) {
						severity = "warning";
						status = ValidationEventStatus::WARNING;
					} else if (has_pass_keyword) {
						severity = "info";
						status = ValidationEventStatus::PASS;
					}

					event.base_event.status = status;
					event.base_event.severity = severity;
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
	}

	return events;
}

// NOTE: Parser registration is handled manually in ReadDuckHuntWorkflowLogInitGlobal
// to avoid static initialization order issues across platforms.
// Do not use REGISTER_WORKFLOW_PARSER macro here.

} // namespace duckdb
