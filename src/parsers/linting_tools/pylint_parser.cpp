#include "pylint_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Pylint parsing (compiled once, reused)
namespace {
// Validation patterns
static const std::regex RE_PYLINT_MODULE(R"(\*+\s*Module\s+)");
static const std::regex RE_PYLINT_LEGACY_CHECK(R"([CWERF]:\s*\d+,\s*\d+:)");
static const std::regex RE_PYLINT_MODERN_CHECK(R"(\S+\.py:\d+:\d+:\s*[CWERFIB]\d{4}:)");
static const std::regex RE_PYLINT_RATING_CHECK(R"(Your code has been rated at)");

// Parse patterns
static const std::regex RE_PYLINT_MODULE_HEADER(R"(\*+\s*Module\s+(.+))");
static const std::regex RE_PYLINT_LEGACY_MESSAGE(R"(([CWERF]):\s*(\d+),\s*(\d+):\s*(.+?)\s+\(([^)]+)\))");
static const std::regex RE_PYLINT_LEGACY_SIMPLE(R"(([CWERF]):\s*(\d+),\s*(\d+):\s*(.+))");
static const std::regex RE_PYLINT_MODERN_MESSAGE(R"((\S+\.py):(\d+):(\d+):\s*([CWERFIB]\d{4}):\s*(.+?)\s+\(([^)]+)\))");
static const std::regex RE_PYLINT_MODERN_SIMPLE(R"((\S+\.py):(\d+):(\d+):\s*([CWERFIB]\d{4}):\s*(.+))");
static const std::regex RE_PYLINT_RATING(R"(Your code has been rated at ([\d\.-]+)/10)");
} // anonymous namespace

bool PylintParser::canParse(const std::string &content) const {
	// Look for Pylint-specific patterns
	// Modern format: file.py:line:col: CODE: message (symbol)
	// Legacy format: C: line, col: message (symbol)
	if (content.find("Module ") != std::string::npos || content.find("Your code has been rated") != std::string::npos ||
	    // Check for modern pylint format with error codes like C0114, W0612, E1101
	    (content.find(": ") != std::string::npos &&
	     (content.find("C0") != std::string::npos || content.find("C1") != std::string::npos ||
	      content.find("W0") != std::string::npos || content.find("W1") != std::string::npos ||
	      content.find("E0") != std::string::npos || content.find("E1") != std::string::npos ||
	      content.find("R0") != std::string::npos || content.find("R1") != std::string::npos ||
	      content.find("F0") != std::string::npos)) ||
	    // Legacy format detection
	    (content.find(": ") != std::string::npos &&
	     (content.find(" C:") != std::string::npos || content.find(" W:") != std::string::npos ||
	      content.find(" E:") != std::string::npos || content.find(" R:") != std::string::npos ||
	      content.find(" F:") != std::string::npos))) {
		return isValidPylintOutput(content);
	}
	return false;
}

bool PylintParser::isValidPylintOutput(const std::string &content) const {
	// Check for Pylint-specific output patterns
	return std::regex_search(content, RE_PYLINT_MODULE) || std::regex_search(content, RE_PYLINT_LEGACY_CHECK) ||
	       std::regex_search(content, RE_PYLINT_MODERN_CHECK) || std::regex_search(content, RE_PYLINT_RATING_CHECK);
}

std::vector<ValidationEvent> PylintParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 80); // Estimate: ~1 event per 80 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;
	std::string current_module;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Check for module header
		if (std::regex_search(line, match, RE_PYLINT_MODULE_HEADER)) {
			current_module = match[1].str();
			continue;
		}

		// Check for modern Pylint format: file.py:line:col: CODE: message (symbol)
		if (std::regex_search(line, match, RE_PYLINT_MODERN_MESSAGE)) {
			std::string file_path = match[1].str();
			std::string line_str = match[2].str();
			std::string column_str = match[3].str();
			std::string error_code = match[4].str();
			std::string message = match[5].str();
			std::string symbol = match[6].str();

			int64_t line_number = 0;
			int64_t column_number = 0;

			try {
				line_number = SafeParsing::SafeStoi(line_str);
				column_number = SafeParsing::SafeStoi(column_str);
			} catch (...) {
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;

			// Map Pylint severity from error code first char (C, W, E, R, F, I, B)
			char severity_char = error_code[0];
			if (severity_char == 'E' || severity_char == 'F') {
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
				event.event_type = ValidationEventType::BUILD_ERROR;
			} else if (severity_char == 'W') {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			} else {
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
			}

			event.message = message + " (" + symbol + ")";
			event.ref_file = file_path;
			event.ref_line = line_number;
			event.ref_column = column_number;
			event.error_code = error_code;
			event.tool_name = "pylint";
			event.category = "code_quality";
			event.log_content = line;
			event.structured_data = "{\"error_code\": \"" + error_code + "\", \"symbol\": \"" + symbol + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Check for modern format without symbol
		if (std::regex_search(line, match, RE_PYLINT_MODERN_SIMPLE)) {
			std::string file_path = match[1].str();
			std::string line_str = match[2].str();
			std::string column_str = match[3].str();
			std::string error_code = match[4].str();
			std::string message = match[5].str();

			int64_t line_number = 0;
			int64_t column_number = 0;

			try {
				line_number = SafeParsing::SafeStoi(line_str);
				column_number = SafeParsing::SafeStoi(column_str);
			} catch (...) {
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;

			char severity_char = error_code[0];
			if (severity_char == 'E' || severity_char == 'F') {
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
				event.event_type = ValidationEventType::BUILD_ERROR;
			} else if (severity_char == 'W') {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			} else {
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
			}

			event.message = message;
			event.ref_file = file_path;
			event.ref_line = line_number;
			event.ref_column = column_number;
			event.error_code = error_code;
			event.tool_name = "pylint";
			event.category = "code_quality";
			event.log_content = line;
			event.structured_data = "{\"error_code\": \"" + error_code + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Check for legacy Pylint message with error code: C:  1, 0: message (code)
		if (std::regex_search(line, match, RE_PYLINT_LEGACY_MESSAGE)) {
			std::string severity_char = match[1].str();
			std::string line_str = match[2].str();
			std::string column_str = match[3].str();
			std::string message = match[4].str();
			std::string error_code = match[5].str();

			int64_t line_number = 0;
			int64_t column_number = 0;

			try {
				line_number = SafeParsing::SafeStoi(line_str);
				column_number = SafeParsing::SafeStoi(column_str);
			} catch (...) {
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;

			if (severity_char == "E" || severity_char == "F") {
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
				event.event_type = ValidationEventType::BUILD_ERROR;
			} else if (severity_char == "W") {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			} else {
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
			}

			event.message = message;
			event.ref_file = current_module.empty() ? "unknown" : current_module;
			event.ref_line = line_number;
			event.ref_column = column_number;
			event.error_code = error_code;
			event.tool_name = "pylint";
			event.category = "code_quality";
			event.log_content = line;
			event.structured_data =
			    "{\"severity_char\": \"" + severity_char + "\", \"error_code\": \"" + error_code + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Check for legacy Pylint message without explicit error code
		if (std::regex_search(line, match, RE_PYLINT_LEGACY_SIMPLE)) {
			std::string severity_char = match[1].str();
			std::string line_str = match[2].str();
			std::string column_str = match[3].str();
			std::string message = match[4].str();

			int64_t line_number = 0;
			int64_t column_number = 0;

			try {
				line_number = SafeParsing::SafeStoi(line_str);
				column_number = SafeParsing::SafeStoi(column_str);
			} catch (...) {
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;

			if (severity_char == "E" || severity_char == "F") {
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
				event.event_type = ValidationEventType::BUILD_ERROR;
			} else if (severity_char == "W") {
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			} else {
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
			}

			event.message = message;
			event.ref_file = current_module.empty() ? "unknown" : current_module;
			event.ref_line = line_number;
			event.ref_column = column_number;
			event.tool_name = "pylint";
			event.category = "code_quality";
			event.log_content = line;
			event.structured_data = "{\"severity_char\": \"" + severity_char + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Check for rating information
		if (std::regex_search(line, match, RE_PYLINT_RATING)) {
			std::string rating = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Code quality rating: " + rating + "/10";
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "pylint";
			event.category = "code_quality";
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"rating\": \"" + rating + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
