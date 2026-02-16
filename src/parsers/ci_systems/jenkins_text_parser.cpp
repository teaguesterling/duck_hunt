#include "jenkins_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Jenkins text parsing (compiled once, reused)
namespace {
static const std::regex RE_PIPELINE_STEP(R"(\[Pipeline\]\s*(.+))");
static const std::regex RE_STAGE_START(R"(\[Pipeline\]\s*stage\s*\(([^)]+)\))");
static const std::regex RE_NODE_BLOCK(R"(\[Pipeline\]\s*node\s*\{)");
static const std::regex RE_BUILD_RESULT(R"(Finished:\s*(SUCCESS|FAILURE|UNSTABLE|ABORTED))");
static const std::regex RE_STARTED_BY(R"(Started by\s+(.+))");
static const std::regex RE_BUILDING_IN(R"(Building in workspace\s+(.+))");
static const std::regex RE_SHELL_CMD(R"(\[.+\]\s*\$\s*(.+))");
static const std::regex RE_ERROR_LINE(R"(^\s*(ERROR|FATAL|Exception|java\.lang\.\w+Exception):?\s*(.+))");
static const std::regex RE_WARNING_LINE(R"(^\s*WARNING:?\s*(.+))");
static const std::regex RE_STACK_TRACE(R"(^\s+at\s+(\S+)\(([^:]+):(\d+)\))");
static const std::regex RE_BUILD_STEP(R"(^\[(.+)\]\s+(.+))");
static const std::regex RE_JUNIT_RESULT(R"(Tests run:\s*(\d+),\s*Failures:\s*(\d+),\s*Errors:\s*(\d+))");
static const std::regex RE_ARCHIVE_ARTIFACTS(R"(Archiving artifacts)");
} // anonymous namespace

bool JenkinsTextParser::canParse(const std::string &content) const {
	// Jenkins specific markers
	bool has_jenkins_marker =
	    content.find("Started by") != std::string::npos &&
	    (content.find("Building in workspace") != std::string::npos ||
	     content.find("Building on master") != std::string::npos || content.find("Running on") != std::string::npos);

	if (has_jenkins_marker) {
		return true;
	}

	// Jenkins finish markers
	bool has_finish_marker = content.find("Finished: SUCCESS") != std::string::npos ||
	                         content.find("Finished: FAILURE") != std::string::npos ||
	                         content.find("Finished: UNSTABLE") != std::string::npos ||
	                         content.find("Finished: ABORTED") != std::string::npos;

	if (has_finish_marker) {
		return true;
	}

	// Jenkins pipeline markers
	bool has_pipeline = content.find("[Pipeline]") != std::string::npos ||
	                    (content.find("stage(") != std::string::npos && content.find("node(") != std::string::npos);

	if (has_pipeline) {
		return true;
	}

	// Jenkins plugin patterns
	bool has_jenkins_plugin = content.find("at org.jenkinsci.plugins") != std::string::npos ||
	                          content.find("at hudson.") != std::string::npos;

	if (has_jenkins_plugin &&
	    (content.find("Exception") != std::string::npos || content.find("java.lang.") != std::string::npos)) {
		return true;
	}

	return false;
}

std::vector<ValidationEvent> JenkinsTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	std::string current_stage;
	bool in_error_block = false;
	int32_t error_start_line = 0;
	std::vector<std::string> error_lines;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		// Build result
		if (std::regex_search(line, match, RE_BUILD_RESULT)) {
			std::string result = match[1].str();
			bool is_failure = (result == "FAILURE" || result == "UNSTABLE" || result == "ABORTED");

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = is_failure ? "error" : "info";
			event.status = is_failure ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.message = "Build finished: " + result;
			event.tool_name = "jenkins";
			event.category = "jenkins_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Stage start
		else if (std::regex_search(line, match, RE_STAGE_START)) {
			current_stage = match[1].str();
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Starting stage: " + current_stage;
			event.tool_name = "jenkins";
			event.category = "jenkins_text";
			event.scope = current_stage;
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Error/Exception lines
		else if (std::regex_search(line, match, RE_ERROR_LINE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = match[1].str() + ": " + match[2].str();
			event.tool_name = "jenkins";
			event.category = "jenkins_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_stage.empty()) {
				event.scope = current_stage;
			}
			events.push_back(event);
			in_error_block = true;
			error_start_line = line_num;
		}
		// Stack trace line - add file/line info to previous error
		else if (in_error_block && std::regex_search(line, match, RE_STACK_TRACE)) {
			if (!events.empty()) {
				ValidationEvent &last = events.back();
				if (last.ref_file.empty() && last.category == "jenkins_text") {
					last.ref_file = match[2].str();
					last.ref_line = SafeParsing::SafeStoi(match[3].str());
					last.log_line_end = line_num;
				}
			}
		}
		// Warning lines
		else if (std::regex_search(line, match, RE_WARNING_LINE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
			event.message = match[1].str();
			event.tool_name = "jenkins";
			event.category = "jenkins_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_stage.empty()) {
				event.scope = current_stage;
			}
			events.push_back(event);
		}
		// JUnit test results
		else if (std::regex_search(line, match, RE_JUNIT_RESULT)) {
			int tests = SafeParsing::SafeStoi(match[1].str());
			int failures = SafeParsing::SafeStoi(match[2].str());
			int errors = SafeParsing::SafeStoi(match[3].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = (failures > 0 || errors > 0) ? "error" : "info";
			event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.message = "Tests: " + std::to_string(tests) + ", Failures: " + std::to_string(failures) +
			                ", Errors: " + std::to_string(errors);
			event.tool_name = "jenkins";
			event.category = "jenkins_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_stage.empty()) {
				event.scope = current_stage;
			}
			events.push_back(event);
		}
		// Non-stack-trace line ends error block
		else if (in_error_block && !line.empty() && line[0] != '\t' && line.find("at ") != 0) {
			in_error_block = false;
		}
	}

	return events;
}

} // namespace duckdb
