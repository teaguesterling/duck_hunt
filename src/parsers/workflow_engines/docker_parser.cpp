#include "docker_parser.hpp"
#include "read_duck_hunt_workflow_log_function.hpp"
#include "core/parser_registry.hpp"
#include "duckdb/common/string_util.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Docker parsing (compiled once, reused)
namespace {
static const std::regex RE_TAG_PATTERN(R"(Successfully tagged ([^\s]+))");
static const std::regex RE_BUILD_PATTERN(R"(Successfully built ([a-f0-9]+))");
static const std::regex RE_LAYER_PATTERN(R"(---> ([a-f0-9]+))");
} // anonymous namespace

bool DockerParser::canParse(const std::string &content) const {
	// Docker build specific patterns
	return content.find("FROM ") != std::string::npos || content.find("RUN ") != std::string::npos ||
	       content.find("COPY ") != std::string::npos || content.find("Step ") != std::string::npos ||
	       content.find("---> ") != std::string::npos || content.find("Successfully built") != std::string::npos ||
	       content.find("Successfully tagged") != std::string::npos ||
	       content.find("Using cache") != std::string::npos ||
	       content.find("Sending build context") != std::string::npos;
}

std::vector<WorkflowEvent> DockerParser::parseWorkflowLog(const std::string &content) const {
	std::vector<WorkflowEvent> events;

	// Parse builds and stages
	std::vector<DockerBuild> builds = parseBuilds(content);

	// Convert to events
	return convertToEvents(builds);
}

bool DockerParser::isDockerCommand(const std::string &line) const {
	return line.find("Step ") != std::string::npos || line.find("FROM ") != std::string::npos ||
	       line.find("RUN ") != std::string::npos || line.find("COPY ") != std::string::npos ||
	       line.find("ADD ") != std::string::npos || line.find("WORKDIR ") != std::string::npos ||
	       line.find("ENV ") != std::string::npos || line.find("EXPOSE ") != std::string::npos ||
	       line.find("CMD ") != std::string::npos || line.find("ENTRYPOINT ") != std::string::npos;
}

bool DockerParser::isDockerStep(const std::string &line) const {
	return line.find("Step ") == 0 || line.find(" Step ") != std::string::npos;
}

bool DockerParser::isMultiStageFrom(const std::string &line) const {
	return line.find("FROM ") != std::string::npos && line.find(" AS ") != std::string::npos;
}

bool DockerParser::isLayerCache(const std::string &line) const {
	return line.find("Using cache") != std::string::npos || line.find("---> Using cache") != std::string::npos;
}

bool DockerParser::isBuildComplete(const std::string &line) const {
	return line.find("Successfully built") != std::string::npos ||
	       line.find("Successfully tagged") != std::string::npos;
}

std::string DockerParser::extractBuildName(const std::string &content) const {
	// Try to extract from successful build message
	std::smatch match;
	if (std::regex_search(content, match, RE_TAG_PATTERN)) {
		return match[1].str();
	}

	// Default build name
	return "Docker Build";
}

std::string DockerParser::extractBuildId(const std::string &content) const {
	// Try to extract from successful build message
	std::smatch match;
	if (std::regex_search(content, match, RE_BUILD_PATTERN)) {
		return match[1].str();
	}

	// Generate a simple hash-based ID
	std::hash<std::string> hasher;
	return std::to_string(hasher(content.substr(0, 100)) % 1000000);
}

std::string DockerParser::extractStageName(const std::string &line) const {
	if (isMultiStageFrom(line)) {
		size_t as_pos = line.find(" AS ");
		if (as_pos != std::string::npos) {
			size_t start = as_pos + 4;
			size_t end = line.find_first_of(" \r\n", start);
			if (end == std::string::npos)
				end = line.length();
			return line.substr(start, end - start);
		}
	}

	if (line.find("FROM ") != std::string::npos) {
		return "base";
	}

	return "stage";
}

std::string DockerParser::extractBaseImage(const std::string &line) const {
	if (line.find("FROM ") != std::string::npos) {
		size_t start = line.find("FROM ") + 5;
		size_t end = line.find(" AS ", start);
		if (end == std::string::npos) {
			end = line.find_first_of(" \r\n", start);
		}
		if (end == std::string::npos)
			end = line.length();
		return line.substr(start, end - start);
	}
	return "";
}

std::string DockerParser::extractCommand(const std::string &line) const {
	// Extract the Docker command from the line
	std::vector<std::string> commands = {"FROM", "RUN", "COPY", "ADD", "WORKDIR", "ENV", "EXPOSE", "CMD", "ENTRYPOINT"};

	for (const auto &cmd : commands) {
		if (line.find(cmd + " ") != std::string::npos) {
			return cmd;
		}
	}

	if (isDockerStep(line)) {
		// Extract step command
		size_t colon_pos = line.find(": ");
		if (colon_pos != std::string::npos) {
			std::string command_part = line.substr(colon_pos + 2);
			for (const auto &cmd : commands) {
				if (command_part.find(cmd + " ") == 0) {
					return cmd;
				}
			}
		}
	}

	return "UNKNOWN";
}

std::string DockerParser::extractLayerId(const std::string &line) const {
	// Extract layer ID from lines like "---> a1b2c3d4e5f6"
	std::smatch match;
	if (std::regex_search(line, match, RE_LAYER_PATTERN)) {
		return match[1].str();
	}

	// Generate from line content
	std::hash<std::string> hasher;
	return std::to_string(hasher(line) % 1000000);
}

std::string DockerParser::extractStatus(const std::string &line) const {
	if (isBuildComplete(line)) {
		return "success";
	}

	if (line.find("ERROR") != std::string::npos || line.find("FAILED") != std::string::npos ||
	    line.find("error") != std::string::npos) {
		return "failure";
	}

	if (isLayerCache(line)) {
		return "cached";
	}

	if (line.find("---> Running in") != std::string::npos) {
		return "running";
	}

	if (line.find("---> ") != std::string::npos) {
		return "completed";
	}

	return "running";
}

std::string DockerParser::extractRunCommand(const std::string &line) const {
	// Extract the full RUN command (what comes after "RUN ")
	// Handles both "Step N/M : RUN command" and standalone "RUN command" formats
	size_t run_pos = line.find("RUN ");
	if (run_pos != std::string::npos) {
		std::string command = line.substr(run_pos + 4);
		// Trim trailing whitespace/newlines
		size_t end = command.find_last_not_of(" \t\r\n");
		if (end != std::string::npos) {
			command = command.substr(0, end + 1);
		}
		return command;
	}
	return "";
}

std::vector<DockerParser::DockerBuild> DockerParser::parseBuilds(const std::string &content) const {
	std::vector<DockerBuild> builds;
	DockerBuild current_build;
	current_build.build_name = extractBuildName(content);
	current_build.build_id = extractBuildId(content);
	current_build.dockerfile_path = "Dockerfile";
	current_build.context_path = ".";

	std::istringstream iss(content);
	std::string line;
	std::vector<std::string> current_stage_lines;
	std::string current_stage_name = "default";

	while (std::getline(iss, line)) {
		if (line.find("FROM ") != std::string::npos) {
			// Starting a new stage
			if (!current_stage_lines.empty()) {
				// Finish previous stage
				current_build.stages.push_back(parseStage(current_stage_lines, current_stage_name));
				current_stage_lines.clear();
			}
			current_stage_name = extractStageName(line);
			current_stage_lines.push_back(line);
		} else {
			current_stage_lines.push_back(line);
		}
	}

	// Handle remaining stage if any
	if (!current_stage_lines.empty()) {
		current_build.stages.push_back(parseStage(current_stage_lines, current_stage_name));
	}

	// If no stages were found, create a default stage
	if (current_build.stages.empty()) {
		DockerStage default_stage;
		default_stage.stage_name = "Docker Build";
		default_stage.stage_id = "stage_1";
		default_stage.base_image = "unknown";
		default_stage.status = "success";

		DockerLayer default_layer;
		default_layer.layer_id = "layer_1";
		default_layer.command = "BUILD";
		default_layer.status = "success";
		default_layer.output_lines = {content.substr(0, std::min(size_t(500), content.length()))};
		default_stage.layers.push_back(default_layer);

		current_build.stages.push_back(default_stage);
	}

	builds.push_back(current_build);
	return builds;
}

DockerParser::DockerStage DockerParser::parseStage(const std::vector<std::string> &stage_lines,
                                                   const std::string &stage_name) const {
	DockerStage stage;
	stage.stage_name = stage_name;
	stage.status = "success";

	// Generate stage ID
	std::hash<std::string> hasher;
	stage.stage_id = "stage_" + std::to_string(hasher(stage_name) % 10000);

	// Extract base image from FROM command
	for (const auto &line : stage_lines) {
		if (line.find("FROM ") != std::string::npos) {
			stage.base_image = extractBaseImage(line);
			break;
		}
	}

	// Parse layers (each Docker command becomes a layer)
	std::vector<std::string> current_layer_lines;
	std::string current_command;

	for (const auto &line : stage_lines) {
		if (isDockerCommand(line)) {
			// Starting a new layer
			if (!current_layer_lines.empty() && !current_command.empty()) {
				stage.layers.push_back(parseLayer(current_layer_lines, current_command));
				current_layer_lines.clear();
			}
			current_command = extractCommand(line);
			current_layer_lines.push_back(line);
		} else {
			current_layer_lines.push_back(line);
		}
	}

	// Handle remaining layer if any
	if (!current_layer_lines.empty() && !current_command.empty()) {
		stage.layers.push_back(parseLayer(current_layer_lines, current_command));
	}

	// Determine overall stage status
	for (const auto &layer : stage.layers) {
		if (layer.status == "failure") {
			stage.status = "failure";
			break;
		}
	}

	return stage;
}

DockerParser::DockerLayer DockerParser::parseLayer(const std::vector<std::string> &layer_lines,
                                                   const std::string &command) const {
	DockerLayer layer;
	layer.command = command;
	layer.status = "success"; // Default

	// Generate layer ID
	if (!layer_lines.empty()) {
		layer.layer_id = extractLayerId(layer_lines[0]);
	} else {
		std::hash<std::string> hasher;
		layer.layer_id = "layer_" + std::to_string(hasher(command) % 10000);
	}

	// Determine layer status
	for (const auto &line : layer_lines) {
		std::string line_status = extractStatus(line);
		if (line_status == "failure") {
			layer.status = "failure";
			break;
		} else if (line_status == "cached" && layer.status != "failure") {
			layer.status = "cached";
		}
	}

	layer.output_lines = layer_lines;

	// Try to find command for delegation (only for RUN commands)
	if (command == "RUN") {
		for (const auto &line : layer_lines) {
			std::string run_command = extractRunCommand(line);
			if (!run_command.empty()) {
				layer.detected_command = run_command;
				break;
			}
		}

		// Try to delegate parsing to a specialized parser based on command
		if (!layer.detected_command.empty()) {
			auto &registry = ParserRegistry::getInstance();
			IParser *delegated_parser = registry.findParserByCommand(layer.detected_command);

			if (delegated_parser) {
				layer.delegated_format = delegated_parser->getFormatName();

				// Build content string from output lines (skip the RUN line and metadata)
				std::string layer_content;
				bool found_run = false;
				for (const auto &line : layer_lines) {
					// Skip lines before and including the RUN command
					if (!found_run) {
						if (line.find("RUN ") != std::string::npos) {
							found_run = true;
						}
						continue;
					}
					// Skip Docker metadata lines
					if (line.find("---> ") != std::string::npos) {
						continue;
					}
					if (line.find("Removing intermediate container") != std::string::npos) {
						continue;
					}
					layer_content += line + "\n";
				}

				// Parse with delegated parser
				if (!layer_content.empty()) {
					try {
						layer.delegated_events = delegated_parser->parse(layer_content);
					} catch (const std::exception &) {
						// Delegation failed, will fall back to workflow-level parsing
						layer.delegated_events.clear();
					}
				}
			}
		}
	}

	return layer;
}

std::vector<WorkflowEvent> DockerParser::convertToEvents(const std::vector<DockerBuild> &builds) const {
	std::vector<WorkflowEvent> events;

	for (const auto &build : builds) {
		for (const auto &stage : build.stages) {
			for (const auto &layer : stage.layers) {
				// If we have delegated events, use those instead of raw output lines
				if (!layer.delegated_events.empty()) {
					for (const auto &delegated_event : layer.delegated_events) {
						WorkflowEvent event;
						event.base_event = delegated_event;

						// Enrich with workflow context
						event.base_event.scope = "Docker Build";
						event.base_event.group = stage.stage_name;
						event.base_event.unit = layer.command;
						event.base_event.scope_id = build.build_id;
						event.base_event.group_id = stage.stage_id;
						event.base_event.unit_id = layer.layer_id;
						event.base_event.scope_status = "running";
						event.base_event.group_status = stage.status;
						event.base_event.unit_status = layer.status;
						if (event.base_event.started_at.empty()) {
							event.base_event.started_at = layer.started_at;
						}
						event.base_event.origin = stage.base_image;
						event.base_event.ref_file = build.dockerfile_path;

						// Mark as delegated
						event.workflow_type = "docker_build";
						event.hierarchy_level = 4; // Delegated tool event level
						event.parent_id = layer.layer_id;

						// Add delegation info to structured_data
						if (!layer.delegated_format.empty()) {
							event.base_event.structured_data = layer.delegated_format;
						}

						events.push_back(event);
					}
				} else {
					// No delegation - emit only meaningful workflow events
					for (const auto &output_line : layer.output_lines) {
						// Filter to meaningful lines only
						bool is_docker_cmd = output_line.find("FROM ") != std::string::npos ||
						                     output_line.find("RUN ") != std::string::npos ||
						                     output_line.find("COPY ") != std::string::npos ||
						                     output_line.find("ADD ") != std::string::npos ||
						                     output_line.find("WORKDIR ") != std::string::npos;
						bool is_step = output_line.find("Step ") != std::string::npos;
						bool is_layer_id = output_line.find("---> ") != std::string::npos;
						bool is_cache = output_line.find("Using cache") != std::string::npos;
						bool is_complete = output_line.find("Successfully built") != std::string::npos ||
						                   output_line.find("Successfully tagged") != std::string::npos;
						bool has_error = output_line.find("ERROR") != std::string::npos ||
						                 output_line.find("error:") != std::string::npos;
						bool has_warning = output_line.find("WARNING") != std::string::npos ||
						                   output_line.find("warning:") != std::string::npos;

						bool is_meaningful = is_docker_cmd || is_step || is_layer_id || is_cache || is_complete ||
						                     has_error || has_warning;

						if (!is_meaningful) {
							continue;
						}

						WorkflowEvent event;

						// Create base validation event
						ValidationEvent base_event =
						    createBaseEvent(output_line, "Docker Build", stage.stage_name, layer.command);

						// Set the base_event
						event.base_event = base_event;

						// Override specific fields in base_event (Schema V2)
						event.base_event.status = ValidationEventStatus::INFO;
						event.base_event.severity = determineSeverity(layer.status, output_line);
						event.base_event.ref_file = build.dockerfile_path;
						event.base_event.function_name = layer.command;
						event.base_event.category = "docker_build";
						event.base_event.scope = "Docker Build";
						event.base_event.group = stage.stage_name;
						event.base_event.unit = layer.command;
						event.base_event.scope_id = build.build_id;
						event.base_event.group_id = stage.stage_id;
						event.base_event.unit_id = layer.layer_id;
						event.base_event.scope_status = "running";
						event.base_event.group_status = stage.status;
						event.base_event.unit_status = layer.status;
						event.base_event.started_at = layer.started_at;
						event.base_event.origin = stage.base_image;
						event.workflow_type = "docker_build";
						event.hierarchy_level = 3; // Layer level (equivalent to step)
						event.parent_id = stage.stage_id;

						events.push_back(event);
					}
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
