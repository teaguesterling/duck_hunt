#include "pytest_parser.hpp"
#include <sstream>
#include <regex>
#include <unordered_map>

namespace duckdb {

// Structure to hold failure location info extracted from FAILURES section
struct FailureInfo {
	std::string file;
	int32_t line;
	std::string error_type;
	std::string error_message;
};

bool PytestParser::canParse(const std::string &content) const {
	// Check for pytest text patterns (file.py::test_name with PASSED/FAILED/SKIPPED)
	return content.find("::") != std::string::npos &&
	       (content.find("PASSED") != std::string::npos || content.find("FAILED") != std::string::npos ||
	        content.find("SKIPPED") != std::string::npos);
}

// Extract failure info from the FAILURES section
// Returns a map from test_name -> FailureInfo
static std::unordered_map<std::string, FailureInfo> extractFailureInfo(const std::string &content) {
	std::unordered_map<std::string, FailureInfo> failures;
	std::istringstream stream(content);
	std::string line;

	// Regex patterns
	// Failure block header: "_____ test_name _____" or "_____ test_name[param] _____"
	static const std::regex header_regex(R"(^_+\s+(\S+)\s+_+$)");
	// Error location: "file.py:line: ErrorType" or "file.py:line:col: ErrorType"
	static const std::regex location_regex(R"(^(\S+\.py):(\d+)(?::\d+)?:\s*(.+)$)");
	// E line with error message: "E       AssertionError: message"
	static const std::regex error_line_regex(R"(^E\s+(.+)$)");

	bool in_failures_section = false;
	std::string current_test_name;
	std::string current_error_message;

	while (std::getline(stream, line)) {
		// Check for FAILURES section start
		if (line.find("= FAILURES =") != std::string::npos ||
		    line.find("=FAILURES=") != std::string::npos ||
		    (line.find("FAILURES") != std::string::npos && line.find("===") != std::string::npos)) {
			in_failures_section = true;
			continue;
		}

		// Check for end of FAILURES section (short test summary or final summary)
		if (in_failures_section &&
		    (line.find("short test summary") != std::string::npos ||
		     line.find("passed") != std::string::npos && line.find("===") != std::string::npos)) {
			in_failures_section = false;
			continue;
		}

		if (!in_failures_section) {
			continue;
		}

		std::smatch match;

		// Check for failure block header
		if (std::regex_search(line, match, header_regex)) {
			current_test_name = match[1].str();
			current_error_message.clear();
			continue;
		}

		// Capture error message from E lines
		if (std::regex_search(line, match, error_line_regex)) {
			if (current_error_message.empty()) {
				current_error_message = match[1].str();
			}
			continue;
		}

		// Check for error location line
		if (!current_test_name.empty() && std::regex_search(line, match, location_regex)) {
			FailureInfo info;
			info.file = match[1].str();
			info.line = std::stoi(match[2].str());
			info.error_type = match[3].str();
			info.error_message = current_error_message;
			failures[current_test_name] = info;
			// Don't clear current_test_name - there might be multiple location lines
			// but we want the last one (closest to the actual failure)
		}
	}

	return failures;
}

std::vector<ValidationEvent> PytestParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	// First, extract failure info from FAILURES section
	auto failure_info = extractFailureInfo(content);

	// Regex for pytest summary line: ===== N passed, N failed, N skipped in X.XXs =====
	static const std::regex summary_regex(
	    R"(=+\s*(\d+)\s+passed(?:,\s*(\d+)\s+failed)?(?:,\s*(\d+)\s+skipped)?.*?in\s+([\d.]+)s?\s*=+)",
	    std::regex::icase);

	while (std::getline(stream, line)) {
		current_line_num++;
		if (line.empty())
			continue;

		// Check for summary line first
		std::smatch match;
		if (std::regex_search(line, match, summary_regex)) {
			ValidationEvent summary;
			summary.event_id = event_id++;
			summary.event_type = ValidationEventType::SUMMARY;
			summary.tool_name = "pytest";
			summary.category = "test_summary";
			summary.ref_file = "";
			summary.ref_line = -1;
			summary.ref_column = -1;
			summary.execution_time = 0.0;
			summary.log_line_start = current_line_num;
			summary.log_line_end = current_line_num;
			summary.log_content = line;

			int passed = std::stoi(match[1].str());
			int failed = match[2].matched ? std::stoi(match[2].str()) : 0;
			int skipped = match[3].matched ? std::stoi(match[3].str()) : 0;
			std::string duration_str = match[4].str();

			summary.message = std::to_string(passed) + " passed";
			if (failed > 0)
				summary.message += ", " + std::to_string(failed) + " failed";
			if (skipped > 0)
				summary.message += ", " + std::to_string(skipped) + " skipped";
			summary.message += " in " + duration_str + "s";

			// If failures, mark as error severity
			if (failed > 0) {
				summary.status = ValidationEventStatus::ERROR;
				summary.severity = "error";
			} else {
				summary.status = ValidationEventStatus::INFO;
				summary.severity = "info";
			}

			summary.structured_data = "{\"passed\":" + std::to_string(passed) +
			                          ",\"failed\":" + std::to_string(failed) +
			                          ",\"skipped\":" + std::to_string(skipped) + ",\"duration\":" + duration_str + "}";

			events.push_back(summary);
			continue;
		}

		// Look for pytest text output patterns: "file.py::test_name STATUS"
		if (line.find("::") != std::string::npos) {
			parseTestLine(line, event_id, events, current_line_num, failure_info);
		}
	}

	return events;
}

void PytestParser::parseTestLine(const std::string &line, int64_t &event_id, std::vector<ValidationEvent> &events,
                                 int32_t log_line_num,
                                 const std::unordered_map<std::string, FailureInfo> &failure_info) const {
	// Parse pytest test result line using simple string parsing
	// Format 1: "file.py::test_name STATUS" or "file.py::test_name STATUS [extra]"
	// Format 2: "STATUS file.py::test_name - message" (short test summary)
	size_t separator = line.find("::");
	if (separator == std::string::npos) {
		return;
	}

	ValidationEvent event;
	event.event_id = event_id++;
	event.tool_name = "pytest";
	event.event_type = ValidationEventType::TEST_RESULT;
	event.ref_line = -1;
	event.ref_column = -1;
	event.execution_time = 0.0;
	event.category = "test";
	event.log_content = line;
	event.structured_data = "pytest_text";
	event.log_line_start = log_line_num;
	event.log_line_end = log_line_num;

	// Check for Format 2: "STATUS file.py::test_name - message" (status at start)
	bool status_at_start = false;
	if (line.rfind("FAILED ", 0) == 0) {
		event.status = ValidationEventStatus::FAIL;
		event.severity = "error";
		status_at_start = true;
	} else if (line.rfind("PASSED ", 0) == 0) {
		event.status = ValidationEventStatus::PASS;
		event.severity = "info";
		status_at_start = true;
	} else if (line.rfind("SKIPPED ", 0) == 0) {
		event.status = ValidationEventStatus::SKIP;
		event.severity = "warning";
		status_at_start = true;
	} else if (line.rfind("ERROR ", 0) == 0) {
		event.status = ValidationEventStatus::ERROR;
		event.severity = "error";
		status_at_start = true;
	}

	if (status_at_start) {
		// Format 2: "STATUS file.py::test_name - message"
		// Find the file path (starts after "STATUS ")
		size_t file_start = line.find(' ') + 1;
		event.ref_file = line.substr(file_start, separator - file_start);

		// Find the test name (between :: and " - " or end)
		std::string rest = line.substr(separator + 2);
		size_t dash_pos = rest.find(" - ");
		if (dash_pos != std::string::npos) {
			event.test_name = rest.substr(0, dash_pos);
			event.message = rest.substr(dash_pos + 3); // Get the message after " - "
		} else {
			event.test_name = rest;
			event.message = "Test " + std::string(event.status == ValidationEventStatus::FAIL   ? "failed"
			                                      : event.status == ValidationEventStatus::PASS ? "passed"
			                                      : event.status == ValidationEventStatus::SKIP ? "skipped"
			                                                                                    : "error");
		}
	} else {
		// Format 1: "file.py::test_name STATUS [extra]"
		event.ref_file = line.substr(0, separator);
		std::string rest = line.substr(separator + 2);

		// Find the status at the end
		if (rest.find(" PASSED") != std::string::npos) {
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.message = "Test passed";
			event.test_name = rest.substr(0, rest.find(" PASSED"));
		} else if (rest.find(" FAILED") != std::string::npos) {
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			event.message = "Test failed";
			event.test_name = rest.substr(0, rest.find(" FAILED"));
		} else if (rest.find(" ERROR") != std::string::npos) {
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.message = "Test error";
			event.test_name = rest.substr(0, rest.find(" ERROR"));
		} else if (rest.find(" SKIPPED") != std::string::npos) {
			event.status = ValidationEventStatus::SKIP;
			event.severity = "warning";
			event.message = "Test skipped";
			event.test_name = rest.substr(0, rest.find(" SKIPPED"));
		} else {
			// No recognized status - skip this line
			event_id--; // Undo the increment
			return;
		}
	}

	// Look up failure info to get line number for failed tests
	if (event.status == ValidationEventStatus::FAIL || event.status == ValidationEventStatus::ERROR) {
		auto it = failure_info.find(event.test_name);
		if (it != failure_info.end()) {
			event.ref_line = it->second.line;
			// Update ref_file from failure info if it's more specific
			if (!it->second.file.empty()) {
				event.ref_file = it->second.file;
			}
			// Enhance message with error details if available
			if (!it->second.error_message.empty() && event.message == "Test failed") {
				event.message = it->second.error_message;
			}
		}
	}

	events.push_back(event);
}

} // namespace duckdb
