#include "shellcheck_text_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for ShellCheck parsing (compiled once, reused)
namespace {
// canParse patterns
static const std::regex RE_IN_LINE_PATTERN(R"(In \S+ line \d+:)");
static const std::regex RE_SC_CODE_PATTERN(R"(SC\d{4})");

// parse patterns
static const std::regex RE_HEADER_PATTERN(R"(^In (\S+) line (\d+):$)");
static const std::regex RE_SC_PATTERN(R"(SC(\d{4})(?:\s*\((\w+)\))?:\s*(.+))");
} // anonymous namespace

bool ShellcheckTextParser::canParse(const std::string &content) const {
	// Look for ShellCheck-specific patterns:
	// 1. "In filename line N:" header
	// 2. SC codes like SC2086, SC2046

	// Check for "In X line Y:" pattern
	if (std::regex_search(content, RE_IN_LINE_PATTERN)) {
		// Verify we also have SC codes
		if (std::regex_search(content, RE_SC_CODE_PATTERN)) {
			return true;
		}
	}

	return false;
}

std::vector<ValidationEvent> ShellcheckTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::string current_file;
	int32_t current_ref_line = 0;
	int32_t current_ref_column = 0;
	std::string pending_code_line;
	int32_t header_line_num = 0;

	int error_count = 0;
	int warning_count = 0;
	int info_count = 0;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Check for file/line header
		if (std::regex_match(line, match, RE_HEADER_PATTERN)) {
			current_file = match[1].str();
			try {
				current_ref_line = std::stoi(match[2].str());
			} catch (...) {
				current_ref_line = 0;
			}
			current_ref_column = 0;
			header_line_num = current_line_num;
			continue;
		}

		// Check for caret position indicator (^--^) to get column
		size_t caret_pos = line.find('^');
		if (caret_pos != std::string::npos && caret_pos < line.size()) {
			current_ref_column = static_cast<int32_t>(caret_pos + 1);
		}

		// Check for SC code
		if (std::regex_search(line, match, RE_SC_PATTERN)) {
			std::string sc_code = "SC" + match[1].str();
			std::string severity_str = match[2].str();
			std::string message = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.tool_name = "shellcheck";
			event.ref_file = current_file;
			event.ref_line = current_ref_line;
			event.ref_column = current_ref_column;
			event.message = message;
			event.error_code = sc_code;
			event.category = "lint";

			// Determine severity from explicit label or SC code range
			if (severity_str == "error" || severity_str.empty()) {
				// Default to warning, check SC code range for errors
				// SC1xxx = parsing errors, SC2xxx = semantic issues
				int code_num = std::stoi(match[1].str());
				if (code_num < 2000) {
					event.severity = "error";
					event.status = ValidationEventStatus::ERROR;
					error_count++;
				} else {
					event.severity = "warning";
					event.status = ValidationEventStatus::WARNING;
					warning_count++;
				}
			} else if (severity_str == "warning") {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
				warning_count++;
			} else if (severity_str == "info" || severity_str == "style") {
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
				info_count++;
			} else {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
				warning_count++;
			}

			event.log_line_start = header_line_num;
			event.log_line_end = current_line_num;
			event.structured_data = "{\"code\": \"" + sc_code + "\"}";

			events.push_back(event);
		}
	}

	// Add summary event
	ValidationEvent summary;
	summary.event_id = event_id++;
	summary.event_type = ValidationEventType::SUMMARY;
	summary.tool_name = "shellcheck";
	summary.category = "lint_summary";
	summary.ref_file = "";
	summary.ref_line = -1;
	summary.ref_column = -1;

	size_t total_issues = events.size();
	if (total_issues == 0) {
		summary.status = ValidationEventStatus::INFO;
		summary.severity = "info";
		summary.message = "No issues found";
	} else if (error_count > 0) {
		summary.status = ValidationEventStatus::ERROR;
		summary.severity = "error";
		summary.message = std::to_string(total_issues) + " issue(s) found";
	} else if (warning_count > 0) {
		summary.status = ValidationEventStatus::WARNING;
		summary.severity = "warning";
		summary.message = std::to_string(total_issues) + " issue(s) found";
	} else {
		summary.status = ValidationEventStatus::INFO;
		summary.severity = "info";
		summary.message = std::to_string(total_issues) + " issue(s) found";
	}

	summary.structured_data =
	    "{\"total\": " + std::to_string(total_issues) + ", \"errors\": " + std::to_string(error_count) +
	    ", \"warnings\": " + std::to_string(warning_count) + ", \"info\": " + std::to_string(info_count) + "}";
	events.push_back(summary);

	return events;
}

} // namespace duckdb
