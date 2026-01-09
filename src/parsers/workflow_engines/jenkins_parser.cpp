#include "jenkins_parser.hpp"
#include "read_duck_hunt_workflow_log_function.hpp"
#include "core/parser_registry.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>

namespace duckdb {

bool JenkinsParser::canParse(const std::string &content) const {
	// Jenkins specific patterns
	return content.find("Started by") != std::string::npos ||
	       content.find("Building in workspace") != std::string::npos ||
	       content.find("Finished: SUCCESS") != std::string::npos ||
	       content.find("Finished: FAILURE") != std::string::npos ||
	       content.find("Console Output") != std::string::npos || content.find("[Pipeline]") != std::string::npos ||
	       (content.find("[") != std::string::npos && content.find("]") != std::string::npos &&
	        (content.find("Build") != std::string::npos || content.find("Job") != std::string::npos));
}

std::vector<WorkflowEvent> JenkinsParser::parseWorkflowLog(const std::string &content) const {
	std::vector<WorkflowEvent> events;

	// Extract workflow metadata
	std::string workflow_name = extractWorkflowName(content);

	// Parse builds and steps
	std::vector<JenkinsBuild> builds = parseBuilds(content);

	// Convert to events
	return convertToEvents(builds, workflow_name);
}

bool JenkinsParser::isJenkinsConsole(const std::string &line) const {
	return line.find("Console Output") != std::string::npos || line.find("Started by") != std::string::npos;
}

bool JenkinsParser::isBuildStart(const std::string &line) const {
	return line.find("Started by") != std::string::npos || line.find("Building in workspace") != std::string::npos ||
	       line.find("Checking out Revision") != std::string::npos;
}

bool JenkinsParser::isBuildEnd(const std::string &line) const {
	return line.find("Finished: ") != std::string::npos;
}

bool JenkinsParser::isStepMarker(const std::string &line) const {
	return line.find("[Pipeline]") != std::string::npos || line.find("+ ") == 0 || // Shell command execution
	       line.find("$ ") == 0 ||                                                 // Command execution
	       (line.find("[") == 0 && line.find("]") != std::string::npos);           // Generic step marker
}

bool JenkinsParser::isWorkspaceInfo(const std::string &line) const {
	return line.find("Building in workspace") != std::string::npos;
}

std::string JenkinsParser::extractWorkflowName(const std::string &content) const {
	// Try to extract from build context
	std::istringstream iss(content);
	std::string line;

	while (std::getline(iss, line)) {
		if (line.find("Started by") != std::string::npos) {
			return "Jenkins Build";
		}
		if (line.find("[Pipeline]") != std::string::npos) {
			return "Jenkins Pipeline";
		}
	}

	// Default workflow name
	return "Jenkins Job";
}

std::string JenkinsParser::extractBuildId(const std::string &content) const {
	// Jenkins doesn't usually include build ID in console logs
	// Generate a simple hash-based ID
	std::hash<std::string> hasher;
	return std::to_string(hasher(content.substr(0, 100)) % 100000);
}

std::string JenkinsParser::extractBuildNumber(const std::string &content) const {
	std::regex build_pattern(R"(Build #(\d+))");
	std::smatch match;
	if (std::regex_search(content, match, build_pattern)) {
		return match[1].str();
	}

	// Generate a simple number
	std::hash<std::string> hasher;
	return std::to_string(hasher(content.substr(0, 50)) % 1000);
}

std::string JenkinsParser::extractBuildName(const std::string &line) const {
	// Extract build name from context
	if (line.find("Building in workspace") != std::string::npos) {
		return "main-build";
	}
	return "jenkins-build";
}

std::string JenkinsParser::extractStepName(const std::string &line) const {
	if (line.find("[Pipeline]") != std::string::npos) {
		size_t start = line.find("[Pipeline] ") + 11;
		size_t end = line.find_first_of("\r\n", start);
		if (end == std::string::npos)
			end = line.length();
		return line.substr(start, end - start);
	}

	if (line.find("+ ") == 0) {
		return "shell_command";
	}

	if (line.find("$ ") == 0) {
		return "command_execution";
	}

	if (line.find("Building in workspace") != std::string::npos) {
		return "workspace_setup";
	}

	if (line.find("Checking out Revision") != std::string::npos) {
		return "git_checkout";
	}

	return "build_step";
}

std::string JenkinsParser::extractWorkspace(const std::string &content) const {
	std::regex workspace_pattern(R"(Building in workspace (.+))");
	std::smatch match;
	if (std::regex_search(content, match, workspace_pattern)) {
		return match[1].str();
	}
	return "";
}

std::string JenkinsParser::extractTimestamp(const std::string &line) const {
	// Jenkins timestamps are often in brackets like [2023-10-15 14:30:15]
	std::regex timestamp_pattern(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\])");
	std::smatch match;
	if (std::regex_search(line, match, timestamp_pattern)) {
		return match.str();
	}

	// Alternative format: simple time [14:30:15]
	std::regex time_pattern(R"(\[\d{2}:\d{2}:\d{2}\])");
	if (std::regex_search(line, match, time_pattern)) {
		return match.str();
	}

	return "";
}

std::string JenkinsParser::extractStatus(const std::string &line) const {
	if (line.find("Finished: SUCCESS") != std::string::npos) {
		return "success";
	}

	if (line.find("Finished: FAILURE") != std::string::npos || line.find("FAILED") != std::string::npos ||
	    line.find("ERROR") != std::string::npos) {
		return "failure";
	}

	if (line.find("Finished: ABORTED") != std::string::npos || line.find("ABORTED") != std::string::npos) {
		return "cancelled";
	}

	if (line.find("WARNING") != std::string::npos || line.find("WARN") != std::string::npos) {
		return "warning";
	}

	return "running";
}

std::string JenkinsParser::extractCommandFromLine(const std::string &line) const {
	// Extract command from shell execution lines
	// Pattern: "+ command args..." or "$ command args..."
	std::string trimmed = line;
	size_t start = trimmed.find_first_not_of(" \t");
	if (start != std::string::npos) {
		trimmed = trimmed.substr(start);
	}

	if (trimmed.find("+ ") == 0) {
		return trimmed.substr(2);
	}
	if (trimmed.find("$ ") == 0) {
		return trimmed.substr(2);
	}

	return "";
}

std::vector<JenkinsParser::JenkinsBuild> JenkinsParser::parseBuilds(const std::string &content) const {
	std::vector<JenkinsBuild> builds;
	JenkinsBuild current_build;
	current_build.build_name = "main-build";
	current_build.build_id = extractBuildId(content);
	current_build.build_number = extractBuildNumber(content);
	current_build.status = "running";
	current_build.workspace = extractWorkspace(content);

	std::istringstream iss(content);
	std::string line;
	std::vector<std::string> current_step_lines;
	std::string current_step_name;

	while (std::getline(iss, line)) {
		if (isStepMarker(line)) {
			// Starting a new step
			if (!current_step_lines.empty() && !current_step_name.empty()) {
				// Finish previous step
				current_build.steps.push_back(parseStep(current_step_lines, current_step_name));
				current_step_lines.clear();
			}
			current_step_name = extractStepName(line);
			current_step_lines.push_back(line);
		} else {
			current_step_lines.push_back(line);

			// Check for build status updates
			if (isBuildEnd(line)) {
				current_build.status = extractStatus(line);
			}
		}
	}

	// Handle remaining step if any
	if (!current_step_lines.empty() && !current_step_name.empty()) {
		current_build.steps.push_back(parseStep(current_step_lines, current_step_name));
	}

	// If no steps were found, create a default step
	if (current_build.steps.empty()) {
		JenkinsStep default_step;
		default_step.step_name = "Build Execution";
		default_step.step_id = "step_1";
		default_step.status = "success";
		default_step.output_lines = {content.substr(0, std::min(size_t(500), content.length()))};
		current_build.steps.push_back(default_step);
	}

	builds.push_back(current_build);
	return builds;
}

JenkinsParser::JenkinsStep JenkinsParser::parseStep(const std::vector<std::string> &step_lines,
                                                    const std::string &step_name) const {
	JenkinsStep step;
	step.step_name = step_name;

	// Generate step ID
	std::hash<std::string> hasher;
	step.step_id = "step_" + std::to_string(hasher(step_name) % 10000);

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
		} else if (line_status == "cancelled" && step.status != "failure" && step.status != "warning") {
			step.status = "cancelled";
		}
	}

	step.output_lines = step_lines;

	// Try to find command for delegation
	// Look for "+ command" or "$ command" lines to determine what tool is running
	for (const auto &line : step_lines) {
		std::string command = extractCommandFromLine(line);
		if (!command.empty()) {
			step.detected_command = command;
			break;
		}
	}

	// Try to delegate parsing to a specialized parser based on command
	if (!step.detected_command.empty()) {
		auto &registry = ParserRegistry::getInstance();
		IParser *delegated_parser = registry.findParserByCommand(step.detected_command);

		if (delegated_parser) {
			step.delegated_format = delegated_parser->getFormatName();

			// Build content string from output lines (skip the command line itself)
			std::string step_content;
			bool found_command = false;
			for (const auto &line : step_lines) {
				if (!found_command && !extractCommandFromLine(line).empty()) {
					found_command = true;
					continue; // Skip the command line itself
				}
				if (found_command) {
					step_content += line + "\n";
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

std::vector<WorkflowEvent> JenkinsParser::convertToEvents(const std::vector<JenkinsBuild> &builds,
                                                          const std::string &workflow_name) const {
	std::vector<WorkflowEvent> events;

	for (const auto &build : builds) {
		for (const auto &step : build.steps) {
			// If we have delegated events, use those instead of raw output lines
			if (!step.delegated_events.empty()) {
				for (const auto &delegated_event : step.delegated_events) {
					WorkflowEvent event;
					event.base_event = delegated_event;

					// Enrich with workflow context
					event.base_event.scope = workflow_name;
					event.base_event.group = build.build_name;
					event.base_event.unit = step.step_name;
					event.base_event.scope_id = build.build_id;
					event.base_event.group_id = build.build_id;
					event.base_event.unit_id = step.step_id;
					event.base_event.scope_status = "running";
					event.base_event.group_status = build.status;
					event.base_event.unit_status = step.status;
					if (event.base_event.started_at.empty()) {
						event.base_event.started_at = step.started_at;
					}
					event.base_event.origin = build.workspace;

					// Mark as delegated
					event.workflow_type = "jenkins";
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
					bool is_command = output_line.find("+ ") != std::string::npos ||
					                  output_line.find("$ ") != std::string::npos;
					bool is_pipeline = output_line.find("[Pipeline]") != std::string::npos;
					bool has_error = output_line.find("ERROR") != std::string::npos ||
					                 output_line.find("FAILED") != std::string::npos ||
					                 output_line.find("error:") != std::string::npos;
					bool has_warning = output_line.find("WARNING") != std::string::npos ||
					                   output_line.find("WARN") != std::string::npos ||
					                   output_line.find("warning:") != std::string::npos;
					bool is_status = output_line.find("Finished:") != std::string::npos ||
					                 output_line.find("Started by") != std::string::npos ||
					                 output_line.find("Building in workspace") != std::string::npos;

					bool is_meaningful =
					    is_command || is_pipeline || has_error || has_warning || is_status;

					if (!is_meaningful) {
						continue;
					}

					WorkflowEvent event;

					// Create base validation event
					ValidationEvent base_event =
					    createBaseEvent(output_line, workflow_name, build.build_name, step.step_name);

					// Set the base_event
					event.base_event = base_event;

					// Override specific fields in base_event (Schema V2)
					event.base_event.status = ValidationEventStatus::INFO;
					event.base_event.severity = determineSeverity(step.status, output_line);
					event.base_event.scope = workflow_name;
					event.base_event.group = build.build_name;
					event.base_event.unit = step.step_name;
					event.base_event.scope_id = build.build_id;
					event.base_event.group_id = build.build_id;
					event.base_event.unit_id = step.step_id;
					event.base_event.scope_status = "running";
					event.base_event.group_status = build.status;
					event.base_event.unit_status = step.status;
					event.base_event.started_at = step.started_at;
					event.base_event.origin = build.workspace;
					event.workflow_type = "jenkins";
					event.hierarchy_level = 3; // Step level
					event.parent_id = build.build_id;

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
