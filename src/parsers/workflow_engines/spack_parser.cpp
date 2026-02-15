#include "spack_parser.hpp"
#include "read_duck_hunt_workflow_log_function.hpp"
#include "core/parser_registry.hpp"
#include "duckdb/common/string_util.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Spack parsing (compiled once, reused)
namespace {
static const std::regex RE_PHASE_PATTERN(R"(Executing phase:\s*'([^']+)')");
static const std::regex RE_TIMESTAMP_PATTERN(R"(\[(\d{4}-\d{2}-\d{2}-\d{2}:\d{2}:\d{2}\.\d+)\])");
} // anonymous namespace

bool SpackParser::canParse(const std::string &content) const {
	// Spack-specific patterns:
	// ==> package: Executing phase: 'phase_name'
	// ==> [timestamp] command
	return content.find("==> ") != std::string::npos &&
	       (content.find("Executing phase:") != std::string::npos || content.find("spack-stage") != std::string::npos ||
	        content.find("spack/opt/spack") != std::string::npos);
}

bool SpackParser::isSpackMarker(const std::string &line) const {
	return line.find("==> ") == 0;
}

bool SpackParser::isPhaseMarker(const std::string &line) const {
	return line.find("Executing phase:") != std::string::npos;
}

bool SpackParser::isTimestampedLine(const std::string &line) const {
	// Pattern: ==> [2025-12-14-13:58:04.226532] ...
	return line.find("==> [") == 0 && line.find("]") != std::string::npos;
}

std::string SpackParser::extractPackageName(const std::string &line) const {
	// Pattern: ==> package_name: Executing phase: ...
	if (isPhaseMarker(line)) {
		size_t start = line.find("==> ");
		if (start != std::string::npos) {
			start += 4;
			size_t end = line.find(":", start);
			if (end != std::string::npos) {
				return line.substr(start, end - start);
			}
		}
	}
	return "spack";
}

std::string SpackParser::extractPhaseName(const std::string &line) const {
	// Pattern: ... Executing phase: 'phase_name'
	std::smatch match;
	if (std::regex_search(line, match, RE_PHASE_PATTERN)) {
		return match[1].str();
	}
	return "unknown";
}

std::string SpackParser::extractTimestamp(const std::string &line) const {
	// Pattern: ==> [2025-12-14-13:58:04.226532] ...
	std::smatch match;
	if (std::regex_search(line, match, RE_TIMESTAMP_PATTERN)) {
		return match[1].str();
	}
	return "";
}

std::string SpackParser::extractCommand(const std::string &line) const {
	// For timestamped lines: ==> [timestamp] 'command' args...
	if (isTimestampedLine(line)) {
		size_t bracket_end = line.find("] ");
		if (bracket_end != std::string::npos) {
			std::string rest = line.substr(bracket_end + 2);
			// If it starts with a quote, extract the quoted command
			if (!rest.empty() && rest[0] == '\'') {
				size_t end_quote = rest.find('\'', 1);
				if (end_quote != std::string::npos) {
					return rest.substr(1, end_quote - 1);
				}
			}
			// Otherwise return the first word/path
			size_t space = rest.find(' ');
			if (space != std::string::npos) {
				return rest.substr(0, space);
			}
			return rest;
		}
	}
	return line;
}

std::vector<SpackParser::SpackBuild> SpackParser::parseBuilds(const std::string &content) const {
	std::vector<SpackBuild> builds;
	SpackBuild current_build;
	current_build.status = "success";

	std::istringstream iss(content);
	std::string line;
	std::vector<std::string> current_phase_lines;
	std::string current_package;
	std::string current_phase;
	int phase_count = 0;

	while (std::getline(iss, line)) {
		if (isPhaseMarker(line)) {
			// Save previous phase if any
			if (!current_phase_lines.empty() && !current_phase.empty()) {
				current_build.phases.push_back(parsePhase(current_phase_lines, current_package, current_phase));
				current_phase_lines.clear();
			}

			// Start new phase
			current_package = extractPackageName(line);
			current_phase = extractPhaseName(line);
			if (current_build.package_name.empty()) {
				current_build.package_name = current_package;
			}
			phase_count++;
			current_phase_lines.push_back(line);
		} else {
			current_phase_lines.push_back(line);
		}
	}

	// Save last phase
	if (!current_phase_lines.empty()) {
		if (current_phase.empty()) {
			current_phase = "build";
		}
		current_build.phases.push_back(parsePhase(current_phase_lines, current_package, current_phase));
	}

	// Generate build ID
	std::hash<std::string> hasher;
	current_build.build_id = "spack_" + std::to_string(hasher(content.substr(0, 100)) % 1000000);

	if (current_build.package_name.empty()) {
		current_build.package_name = "spack";
	}

	// Check for errors
	if (content.find("Error:") != std::string::npos || content.find("error:") != std::string::npos ||
	    content.find("FAILED") != std::string::npos) {
		current_build.status = "failure";
	}

	builds.push_back(current_build);
	return builds;
}

SpackParser::SpackPhase SpackParser::parsePhase(const std::vector<std::string> &phase_lines,
                                                const std::string &package_name, const std::string &phase_name) const {
	SpackPhase phase;
	phase.package_name = package_name;
	phase.phase_name = phase_name;
	phase.status = "success";

	// Generate phase ID
	std::hash<std::string> hasher;
	phase.phase_id = "phase_" + std::to_string(hasher(phase_name) % 10000);

	// Extract timestamp from first timestamped line
	for (const auto &line : phase_lines) {
		std::string ts = extractTimestamp(line);
		if (!ts.empty()) {
			phase.started_at = ts;
			break;
		}
	}

	// Check for errors in phase
	for (const auto &line : phase_lines) {
		if (line.find("error:") != std::string::npos || line.find("Error:") != std::string::npos ||
		    line.find("FAILED") != std::string::npos) {
			phase.status = "failure";
			break;
		}
	}

	phase.output_lines = phase_lines;

	// Try to find command for delegation from timestamped lines
	for (const auto &line : phase_lines) {
		if (isTimestampedLine(line)) {
			std::string command = extractCommand(line);
			if (!command.empty() && command != line) {
				phase.detected_command = command;
				break;
			}
		}
	}

	// Try to delegate parsing to a specialized parser based on command
	if (!phase.detected_command.empty()) {
		auto &registry = ParserRegistry::getInstance();
		IParser *delegated_parser = registry.findParserByCommand(phase.detected_command);

		if (delegated_parser) {
			phase.delegated_format = delegated_parser->getFormatName();

			// Build content string from output lines (skip spack marker lines)
			std::string phase_content;
			for (const auto &line : phase_lines) {
				// Skip spack marker lines (==> ...)
				if (isSpackMarker(line)) {
					continue;
				}
				phase_content += line + "\n";
			}

			// Parse with delegated parser
			if (!phase_content.empty()) {
				try {
					phase.delegated_events = delegated_parser->parse(phase_content);
				} catch (const std::exception &) {
					// Delegation failed, will fall back to workflow-level parsing
					phase.delegated_events.clear();
				}
			}
		}
	}

	return phase;
}

std::vector<WorkflowEvent> SpackParser::parseWorkflowLog(const std::string &content) const {
	std::vector<SpackBuild> builds = parseBuilds(content);
	return convertToEvents(builds);
}

std::vector<WorkflowEvent> SpackParser::convertToEvents(const std::vector<SpackBuild> &builds) const {
	std::vector<WorkflowEvent> events;

	for (const auto &build : builds) {
		for (const auto &phase : build.phases) {
			// If we have delegated events, use those instead of raw output lines
			if (!phase.delegated_events.empty()) {
				for (const auto &delegated_event : phase.delegated_events) {
					WorkflowEvent event;
					event.base_event = delegated_event;

					// Enrich with workflow context
					event.base_event.scope = "Spack Build: " + build.package_name;
					event.base_event.group = phase.phase_name;
					event.base_event.unit = "tool";
					event.base_event.scope_id = build.build_id;
					event.base_event.group_id = phase.phase_id;
					event.base_event.scope_status = build.status;
					event.base_event.group_status = phase.status;
					event.base_event.unit_status = "completed";
					if (event.base_event.started_at.empty()) {
						event.base_event.started_at = phase.started_at;
					}
					event.base_event.origin = phase.package_name;
					event.base_event.tool_name = "spack";
					event.base_event.category = "spack_build";

					// Mark as delegated
					event.workflow_type = "spack";
					event.hierarchy_level = 4; // Delegated tool event level
					event.parent_id = phase.phase_id;

					// Add delegation info to structured_data
					if (!phase.delegated_format.empty()) {
						event.base_event.structured_data = phase.delegated_format;
					}

					events.push_back(event);
				}
			} else {
				// No delegation - emit only meaningful workflow events
				for (const auto &output_line : phase.output_lines) {
					// Filter to meaningful lines only
					bool is_phase_marker = isPhaseMarker(output_line);
					bool is_spack_cmd = isSpackMarker(output_line);
					bool has_error = output_line.find("error:") != std::string::npos ||
					                 output_line.find("Error:") != std::string::npos ||
					                 output_line.find("FAILED") != std::string::npos;
					bool has_warning = output_line.find("warning:") != std::string::npos ||
					                   output_line.find("Warning:") != std::string::npos;

					bool is_meaningful = is_phase_marker || is_spack_cmd || has_error || has_warning;

					if (!is_meaningful) {
						continue;
					}

					WorkflowEvent event;

					// Create base validation event
					ValidationEvent base_event =
					    createBaseEvent(output_line, "Spack Build: " + build.package_name, phase.phase_name,
					                    isSpackMarker(output_line) ? "spack" : "tool");

					event.base_event = base_event;

					// Determine severity based on line content
					std::string severity = "info";
					ValidationEventStatus status = ValidationEventStatus::INFO;
					if (output_line.find("error:") != std::string::npos ||
					    output_line.find("Error:") != std::string::npos) {
						severity = "error";
						status = ValidationEventStatus::ERROR;
					} else if (output_line.find("warning:") != std::string::npos ||
					           output_line.find("Warning:") != std::string::npos) {
						severity = "warning";
						status = ValidationEventStatus::WARNING;
					} else if (isPhaseMarker(output_line)) {
						severity = "info";
						status = ValidationEventStatus::INFO;
					}

					event.base_event.status = status;
					event.base_event.severity = severity;
					event.base_event.tool_name = "spack";
					event.base_event.category = "spack_build";
					event.base_event.scope = "Spack Build: " + build.package_name;
					event.base_event.group = phase.phase_name;
					event.base_event.unit = isSpackMarker(output_line) ? "spack" : "tool";
					event.base_event.scope_id = build.build_id;
					event.base_event.group_id = phase.phase_id;
					event.base_event.scope_status = build.status;
					event.base_event.group_status = phase.status;
					event.base_event.unit_status = "completed";
					event.base_event.started_at = phase.started_at;
					event.base_event.origin = phase.package_name;

					// Extract command if this is a timestamped line
					if (isTimestampedLine(output_line)) {
						event.base_event.function_name = extractCommand(output_line);
					}

					event.workflow_type = "spack";
					event.hierarchy_level = 3; // Line level
					event.parent_id = phase.phase_id;

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
