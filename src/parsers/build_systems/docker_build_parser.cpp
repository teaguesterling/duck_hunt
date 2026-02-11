#include "docker_build_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

bool DockerBuildParser::canParse(const std::string &content) const {
	// Docker build step markers
	bool has_step_marker = content.find("Step ") != std::string::npos &&
	                       (content.find("FROM ") != std::string::npos || content.find("RUN ") != std::string::npos ||
	                        content.find("COPY ") != std::string::npos || content.find("ADD ") != std::string::npos);

	if (has_step_marker) {
		return true;
	}

	// BuildKit format
	bool has_buildkit =
	    content.find("#") != std::string::npos &&
	    (content.find("[stage-") != std::string::npos || content.find("[internal]") != std::string::npos ||
	     content.find("DONE") != std::string::npos || content.find("CACHED") != std::string::npos);

	if (has_buildkit && (content.find("FROM") != std::string::npos || content.find("RUN") != std::string::npos)) {
		return true;
	}

	// Docker run patterns
	bool has_docker_run = content.find("---> Running in") != std::string::npos ||
	                      content.find("Removing intermediate container") != std::string::npos;

	if (has_docker_run) {
		return true;
	}

	// Docker error patterns
	bool has_docker_error =
	    (content.find("returned a non-zero code:") != std::string::npos) ||
	    (content.find("failed to solve:") != std::string::npos) ||
	    (content.find("error: failed to solve:") != std::string::npos) ||
	    (content.find("SECURITY SCANNING:") != std::string::npos && content.find("vulnerability") != std::string::npos);

	return has_docker_error;
}

std::vector<ValidationEvent> DockerBuildParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	// Patterns for Docker output
	std::regex step_line(R"(Step\s+(\d+)/(\d+)\s*:\s*(.+))");
	std::regex running_in(R"(---> Running in ([a-f0-9]+))");
	std::regex layer_cached(R"(---> Using cache)");
	std::regex layer_built(R"(---> ([a-f0-9]+))");
	std::regex removing_container(R"(Removing intermediate container ([a-f0-9]+))");
	std::regex build_success(R"(Successfully built ([a-f0-9]+))");
	std::regex build_tagged(R"(Successfully tagged (.+))");
	std::regex exit_code_error(R"(returned a non-zero code:\s*(\d+))");
	std::regex failed_to_solve(R"(failed to solve:?\s*(.+))");
	std::regex error_line(R"(^(error|ERROR):?\s*(.+))");
	std::regex warning_line(R"(^(warning|WARNING|WARN):?\s*(.+))");

	// BuildKit patterns
	std::regex buildkit_step(R"(#(\d+)\s+\[([^\]]+)\]\s*(.+))");
	std::regex buildkit_cached(R"(#(\d+)\s+CACHED)");
	std::regex buildkit_done(R"(#(\d+)\s+DONE\s+([\d.]+)s)");
	std::regex buildkit_error(R"(#(\d+)\s+ERROR:?\s*(.+))");

	// Security scan patterns
	std::regex vuln_found(R"((\d+)\s+(CRITICAL|HIGH|MEDIUM|LOW)\s+vulnerabilit)");

	std::string current_step;
	int current_step_num = 0;
	int total_steps = 0;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		// Traditional Docker build step
		if (std::regex_search(line, match, step_line)) {
			current_step_num = std::stoi(match[1].str());
			total_steps = std::stoi(match[2].str());
			current_step = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Step " + match[1].str() + "/" + match[2].str() + ": " + current_step;
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// BuildKit step
		else if (std::regex_search(line, match, buildkit_step)) {
			current_step = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "[" + match[2].str() + "] " + current_step;
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Exit code error
		else if (std::regex_search(line, match, exit_code_error)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = "Command failed with exit code " + match[1].str();
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Failed to solve error
		else if (std::regex_search(line, match, failed_to_solve)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = "Build failed: " + match[1].str();
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// BuildKit error
		else if (std::regex_search(line, match, buildkit_error)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = match[2].str();
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Generic error line
		else if (std::regex_search(line, match, error_line)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = match[2].str();
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Warning line
		else if (std::regex_search(line, match, warning_line)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
			event.message = match[2].str();
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Security vulnerabilities
		else if (std::regex_search(line, match, vuln_found)) {
			int count = std::stoi(match[1].str());
			std::string severity = match[2].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SECURITY_FINDING;
			event.severity = (severity == "CRITICAL" || severity == "HIGH") ? "error" : "warning";
			event.status = (severity == "CRITICAL" || severity == "HIGH") ? ValidationEventStatus::FAIL
			                                                              : ValidationEventStatus::WARNING;
			event.message = std::to_string(count) + " " + severity + " vulnerabilities found";
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Build success
		else if (std::regex_search(line, match, build_success)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.status = ValidationEventStatus::PASS;
			event.message = "Successfully built image " + match[1].str();
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Build tagged
		else if (std::regex_search(line, match, build_tagged)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.status = ValidationEventStatus::PASS;
			event.message = "Tagged as " + match[1].str();
			event.tool_name = "docker";
			event.category = "docker_build";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
