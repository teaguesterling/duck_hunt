#include "docker_build_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Docker build parsing (compiled once, reused)
namespace {
static const std::regex RE_STEP_LINE(R"(Step\s+(\d+)/(\d+)\s*:\s*(.+))");
static const std::regex RE_RUNNING_IN(R"(---> Running in ([a-f0-9]+))");
static const std::regex RE_LAYER_CACHED(R"(---> Using cache)");
static const std::regex RE_LAYER_BUILT(R"(---> ([a-f0-9]+))");
static const std::regex RE_REMOVING_CONTAINER(R"(Removing intermediate container ([a-f0-9]+))");
static const std::regex RE_BUILD_SUCCESS(R"(Successfully built ([a-f0-9]+))");
static const std::regex RE_BUILD_TAGGED(R"(Successfully tagged (.+))");
static const std::regex RE_EXIT_CODE_ERROR(R"(returned a non-zero code:\s*(\d+))");
static const std::regex RE_FAILED_TO_SOLVE(R"(failed to solve:?\s*(.+))");
static const std::regex RE_ERROR_LINE(R"(^(error|ERROR):?\s*(.+))");
static const std::regex RE_WARNING_LINE(R"(^(warning|WARNING|WARN):?\s*(.+))");
static const std::regex RE_BUILDKIT_STEP(R"(#(\d+)\s+\[([^\]]+)\]\s*(.+))");
static const std::regex RE_BUILDKIT_CACHED(R"(#(\d+)\s+CACHED)");
static const std::regex RE_BUILDKIT_DONE(R"(#(\d+)\s+DONE\s+([\d.]+)s)");
static const std::regex RE_BUILDKIT_ERROR(R"(#(\d+)\s+ERROR:?\s*(.+))");
static const std::regex RE_VULN_FOUND(R"((\d+)\s+(CRITICAL|HIGH|MEDIUM|LOW)\s+vulnerabilit)");
} // anonymous namespace

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
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	std::string current_step;
	int current_step_num = 0;
	int total_steps = 0;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		// Traditional Docker build step
		if (std::regex_search(line, match, RE_STEP_LINE)) {
			current_step_num = SafeParsing::SafeStoi(match[1].str());
			total_steps = SafeParsing::SafeStoi(match[2].str());
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
		else if (std::regex_search(line, match, RE_BUILDKIT_STEP)) {
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
		else if (std::regex_search(line, match, RE_EXIT_CODE_ERROR)) {
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
		else if (std::regex_search(line, match, RE_FAILED_TO_SOLVE)) {
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
		else if (std::regex_search(line, match, RE_BUILDKIT_ERROR)) {
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
		else if (std::regex_search(line, match, RE_ERROR_LINE)) {
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
		else if (std::regex_search(line, match, RE_WARNING_LINE)) {
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
		else if (std::regex_search(line, match, RE_VULN_FOUND)) {
			int count = SafeParsing::SafeStoi(match[1].str());
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
		else if (std::regex_search(line, match, RE_BUILD_SUCCESS)) {
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
		else if (std::regex_search(line, match, RE_BUILD_TAGGED)) {
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
