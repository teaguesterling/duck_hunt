#include "rubocop_text_parser.hpp"
#include <sstream>

namespace duckdb {

bool RubocopTextParser::canParse(const std::string &content) const {
	// Look for RuboCop-specific patterns:
	// 1. "Inspecting X files" header
	// 2. Offense lines with cop names like "Style/StringLiterals"
	// 3. Summary like "X files inspected, Y offenses detected"

	// Check for RuboCop summary pattern
	if (content.find("files inspected") != std::string::npos &&
	    content.find("offense") != std::string::npos) {
		return true;
	}

	// Check for offense pattern: file:line:col: severity: CopDepartment/CopName: message
	std::regex offense_pattern(R"(\S+\.rb:\d+:\d+:\s*[CWEF]:\s*\w+/\w+:)");
	if (std::regex_search(content, offense_pattern)) {
		return true;
	}

	// Check for "Inspecting X files" header
	if (content.find("Inspecting") != std::string::npos &&
	    content.find("file") != std::string::npos) {
		std::regex inspecting_pattern(R"(Inspecting \d+ files?)");
		if (std::regex_search(content, inspecting_pattern)) {
			return true;
		}
	}

	return false;
}

std::vector<ValidationEvent> RubocopTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	// Pattern for offense line: file.rb:line:col: severity: Cop/Name: message
	std::regex offense_pattern(R"(^([^:]+\.rb):(\d+):(\d+):\s*([CWEF]):\s*(\w+/\w+):\s*(.+)$)");

	int error_count = 0;
	int warning_count = 0;
	int convention_count = 0;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		if (std::regex_match(line, match, offense_pattern)) {
			std::string file_path = match[1].str();
			int32_t line_number = 0;
			int32_t column_number = 0;

			try {
				line_number = std::stoi(match[2].str());
				column_number = std::stoi(match[3].str());
			} catch (...) {
				// Keep as 0 if parsing fails
			}

			char severity_char = match[4].str()[0];
			std::string cop_name = match[5].str();
			std::string message = match[6].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.tool_name = "rubocop";
			event.ref_file = file_path;
			event.ref_line = line_number;
			event.ref_column = column_number;
			event.message = message;
			event.error_code = cop_name;
			event.category = "lint";

			// Map RuboCop severity: C=Convention, W=Warning, E=Error, F=Fatal
			switch (severity_char) {
			case 'F':
			case 'E':
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
				error_count++;
				break;
			case 'W':
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
				warning_count++;
				break;
			case 'C':
			default:
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
				convention_count++;
				break;
			}

			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;
			event.structured_data = "{\"cop\": \"" + cop_name + "\", \"severity_code\": \"" +
			                        std::string(1, severity_char) + "\"}";

			events.push_back(event);
		}
	}

	// Add summary event
	ValidationEvent summary;
	summary.event_id = event_id++;
	summary.event_type = ValidationEventType::SUMMARY;
	summary.tool_name = "rubocop";
	summary.category = "lint_summary";
	summary.ref_file = "";
	summary.ref_line = -1;
	summary.ref_column = -1;

	size_t total_issues = events.size();
	if (total_issues == 0) {
		summary.status = ValidationEventStatus::INFO;
		summary.severity = "info";
		summary.message = "No offenses detected";
	} else if (error_count > 0) {
		summary.status = ValidationEventStatus::ERROR;
		summary.severity = "error";
		summary.message = std::to_string(total_issues) + " offense(s) detected";
	} else if (warning_count > 0) {
		summary.status = ValidationEventStatus::WARNING;
		summary.severity = "warning";
		summary.message = std::to_string(total_issues) + " offense(s) detected";
	} else {
		summary.status = ValidationEventStatus::INFO;
		summary.severity = "info";
		summary.message = std::to_string(total_issues) + " convention issue(s) detected";
	}

	summary.structured_data =
	    "{\"total\": " + std::to_string(total_issues) + ", \"errors\": " + std::to_string(error_count) +
	    ", \"warnings\": " + std::to_string(warning_count) +
	    ", \"conventions\": " + std::to_string(convention_count) + "}";
	events.push_back(summary);

	return events;
}

} // namespace duckdb
