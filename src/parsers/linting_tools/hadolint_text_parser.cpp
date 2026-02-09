#include "hadolint_text_parser.hpp"
#include <sstream>

namespace duckdb {

bool HadolintTextParser::canParse(const std::string &content) const {
	// Look for Hadolint-specific patterns:
	// 1. DL/SC codes (DL3006, SC2086, etc.)
	// 2. Dockerfile reference with line number

	// Check for DL codes (Hadolint-specific)
	std::regex dl_pattern(R"(DL\d{4})");
	if (std::regex_search(content, dl_pattern)) {
		return true;
	}

	// Check for Dockerfile:N pattern with warning/error/info
	std::regex dockerfile_pattern(R"(Dockerfile:\d+\s+\w+\s+(error|warning|info|style):)");
	if (std::regex_search(content, dockerfile_pattern)) {
		return true;
	}

	return false;
}

std::vector<ValidationEvent> HadolintTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	// Pattern for hadolint output: Dockerfile:line CODE severity: message
	// or: filename:line CODE severity: message
	std::regex hadolint_pattern(R"(^([^:]+):(\d+)\s+(DL\d{4}|SC\d{4})\s+(error|warning|info|style):\s*(.+)$)");

	// Alternative pattern without severity (some versions)
	std::regex hadolint_pattern_alt(R"(^([^:]+):(\d+)\s+(DL\d{4}|SC\d{4})\s+(.+)$)");

	int error_count = 0;
	int warning_count = 0;
	int info_count = 0;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		bool matched = false;
		std::string file_path, line_str, code, severity_str, message;

		if (std::regex_match(line, match, hadolint_pattern)) {
			file_path = match[1].str();
			line_str = match[2].str();
			code = match[3].str();
			severity_str = match[4].str();
			message = match[5].str();
			matched = true;
		} else if (std::regex_match(line, match, hadolint_pattern_alt)) {
			file_path = match[1].str();
			line_str = match[2].str();
			code = match[3].str();
			message = match[4].str();
			// Infer severity from code prefix
			severity_str = (code[0] == 'D' && code[1] == 'L') ? "warning" : "warning";
			matched = true;
		}

		if (matched) {
			int32_t line_number = 0;
			try {
				line_number = std::stoi(line_str);
			} catch (...) {
				// Keep as 0 if parsing fails
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.tool_name = "hadolint";
			event.ref_file = file_path;
			event.ref_line = line_number;
			event.ref_column = 0;
			event.message = message;
			event.error_code = code;
			event.category = "lint";

			if (severity_str == "error") {
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
				error_count++;
			} else if (severity_str == "warning") {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
				warning_count++;
			} else {
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
				info_count++;
			}

			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			event.structured_data = "{\"code\": \"" + code + "\", \"severity\": \"" + severity_str + "\"}";

			events.push_back(event);
		}
	}

	// Add summary event
	ValidationEvent summary;
	summary.event_id = event_id++;
	summary.event_type = ValidationEventType::SUMMARY;
	summary.tool_name = "hadolint";
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
