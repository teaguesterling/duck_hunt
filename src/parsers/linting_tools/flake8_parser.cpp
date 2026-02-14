#include "flake8_parser.hpp"
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Flake8 parsing (compiled once, reused)
namespace {
static const std::regex RE_FLAKE8_PATTERN(R"(\.py:\d+:\d+:\s*[FEWC]\d{3,})");
static const std::regex RE_FLAKE8_MESSAGE(R"(([^:]+):(\d+):(\d+):\s*([FEWC]\d+)\s*(.+))");
} // anonymous namespace

bool Flake8Parser::canParse(const std::string &content) const {
	// Look for flake8-specific patterns: error codes like F401, E302, W503, C901
	// Note: Must be careful to avoid false positives from IPv6 addresses containing ":C6" etc.
	// Real flake8 codes have 3+ digits (F401, E302, W503, C901)
	return isValidFlake8Output(content);
}

bool Flake8Parser::isValidFlake8Output(const std::string &content) const {
	// Check for flake8 output pattern: file.py:line:column: CODE message
	// Flake8 error codes are a letter followed by 3 digits (F401, E302, W503, C901)
	// This avoids false positives from IPv6 addresses like "FE80:0000:0000:0000:C6B3"
	return std::regex_search(content, RE_FLAKE8_PATTERN);
}

std::vector<ValidationEvent> Flake8Parser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 60); // Estimate: ~1 event per 60 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		if (std::regex_search(line, match, RE_FLAKE8_MESSAGE)) {
			std::string file_path = match[1].str();
			std::string line_str = match[2].str();
			std::string column_str = match[3].str();
			std::string error_code = match[4].str();
			std::string message = match[5].str();

			int64_t line_number = 0;
			int64_t column_number = 0;

			try {
				line_number = std::stoi(line_str);
				column_number = std::stoi(column_str);
			} catch (...) {
				// If parsing fails, keep as 0
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;

			// Map Flake8 error codes to severity
			if (error_code.front() == 'F') {
				// F codes are pyflakes errors (logical errors)
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
				event.event_type = ValidationEventType::BUILD_ERROR;
			} else if (error_code.front() == 'E') {
				// E codes are PEP 8 errors (style errors)
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
			} else if (error_code.front() == 'W') {
				// W codes are PEP 8 warnings
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			} else if (error_code.front() == 'C') {
				// C codes are complexity warnings
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			} else {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			}

			event.message = message;
			event.ref_file = file_path;
			event.ref_line = line_number;
			event.ref_column = column_number;
			event.error_code = error_code;
			event.tool_name = "flake8";
			event.category = "style_guide";
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"error_code\": \"" + error_code + "\", \"error_type\": \"" +
			                        std::string(1, error_code.front()) + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
	}

	// Add summary event with issue count
	ValidationEvent summary;
	summary.event_id = event_id++;
	summary.event_type = ValidationEventType::SUMMARY;
	summary.tool_name = "flake8";
	summary.category = "lint_summary";
	summary.ref_file = "";
	summary.ref_line = -1;
	summary.ref_column = -1;
	summary.execution_time = 0.0;

	size_t issue_count = events.size();
	if (issue_count == 0) {
		summary.status = ValidationEventStatus::INFO;
		summary.severity = "info";
		summary.message = "No issues found";
	} else {
		summary.status = ValidationEventStatus::WARNING;
		summary.severity = "warning";
		summary.message = std::to_string(issue_count) + " issue(s) found";
	}

	summary.structured_data = "{\"issues\":" + std::to_string(issue_count) + "}";
	events.push_back(summary);

	return events;
}

} // namespace duckdb
