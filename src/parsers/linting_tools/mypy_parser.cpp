#include "mypy_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Extract function/class name from mypy error messages when possible.
// Patterns:
//   Argument N to "func_name" ...          → func_name
//   Returning Any from function ...        → (from error code context)
//   "ClassName" has no attribute "method"  → ClassName.method
//   Incompatible return value type (got "X", expected "Y")  → (none)
//   Name "func" is not defined             → func
static std::string ExtractMypyFunctionName(const std::string &message) {
	// Mypy uses ASCII double quotes in messages: Argument 1 to "func" ...
	static const std::regex RE_ARG_TO(R"xx(Argument \d+ to "([^"]+)")xx");
	static const std::regex RE_NAME_NOT_DEFINED(R"xx(Name "([^"]+)" is not defined)xx");
	static const std::regex RE_HAS_NO_ATTR(R"xx("([^"]+)" has no attribute "([^"]+)")xx");
	static const std::regex RE_IN_FUNC(R"xx(function "([^"]+)")xx");

	std::smatch match;
	if (std::regex_search(message, match, RE_ARG_TO)) {
		return match[1].str();
	}
	if (std::regex_search(message, match, RE_HAS_NO_ATTR)) {
		return match[1].str() + "." + match[2].str();
	}
	if (std::regex_search(message, match, RE_IN_FUNC)) {
		return match[1].str();
	}
	if (std::regex_search(message, match, RE_NAME_NOT_DEFINED)) {
		return match[1].str();
	}
	return "";
}

// Pre-compiled regex patterns for mypy parsing (compiled once, reused)
namespace {
// canParse/isValidMypyOutput patterns
static const std::regex RE_CLANG_TIDY_PATTERN(R"([^:]+:\d+:\d+:\s*(error|warning|note):\s*)");
static const std::regex RE_MYPY_PATTERN(R"([^:]+:\d+:\s*(error|warning|note):)");
static const std::regex RE_MYPY_SUMMARY_CHECK(R"(Found \d+ errors? in \d+ files?)");
static const std::regex RE_MYPY_SUCCESS_CHECK(R"(Success: no issues found in \d+ source files?)");

// parse patterns
static const std::regex RE_MYPY_MESSAGE(R"(([^:]+):(\d+):\s*(error|warning|note):\s*(.+?)\s*\[([^\]]+)\])");
static const std::regex RE_MYPY_MESSAGE_NO_CODE(R"(([^:]+):(\d+):\s*(error|warning|note):\s*(.+))");
static const std::regex RE_MYPY_SUMMARY(R"(Found (\d+) errors? in (\d+) files? \(checked (\d+) files?\))");
static const std::regex RE_MYPY_SUCCESS(R"(Success: no issues found in (\d+) source files?)");
} // anonymous namespace

bool MypyParser::canParse(const std::string &content) const {
	// First check for clang-tidy specific patterns and exclude them
	static const std::vector<std::string> clang_tidy_rules = {
	    "readability-", "performance-", "modernize-", "bugprone-",   "cppcoreguidelines-",
	    "google-",      "llvm-",        "misc-",      "portability-"};

	for (const auto &rule : clang_tidy_rules) {
		if (content.find(rule) != std::string::npos) {
			return false; // This is likely clang-tidy output
		}
	}

	// Check for column numbers which are more common in clang-tidy than mypy
	// Guard: only run expensive regex if we have error/warning/note patterns
	if (content.find(": error:") != std::string::npos || content.find(": warning:") != std::string::npos ||
	    content.find(": note:") != std::string::npos) {
		// Process line-by-line to avoid catastrophic backtracking on large files
		std::istringstream stream(content);
		std::string line;
		while (std::getline(stream, line)) {
			if (std::regex_search(line, RE_CLANG_TIDY_PATTERN)) {
				return false; // This looks like clang-tidy format
			}
		}
	}

	// Look for mypy-specific patterns
	if (content.find("error:") != std::string::npos || content.find("warning:") != std::string::npos ||
	    content.find("Success: no issues found") != std::string::npos ||
	    (content.find("Found") != std::string::npos && content.find("errors") != std::string::npos)) {
		return isValidMypyOutput(content);
	}
	return false;
}

bool MypyParser::isValidMypyOutput(const std::string &content) const {
	// Check for mypy-specific output patterns
	// Process line-by-line to avoid catastrophic backtracking on large files
	std::istringstream stream(content);
	std::string line;
	while (std::getline(stream, line)) {
		if (std::regex_search(line, RE_MYPY_PATTERN)) {
			return true;
		}
	}

	// These patterns are bounded and safe to run on full content
	return std::regex_search(content, RE_MYPY_SUMMARY_CHECK) || std::regex_search(content, RE_MYPY_SUCCESS_CHECK);
}

std::vector<ValidationEvent> MypyParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 80); // Estimate: ~1 event per 80 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Check for mypy message with error code
		if (std::regex_search(line, match, RE_MYPY_MESSAGE)) {
			std::string file_path = match[1].str();
			std::string line_str = match[2].str();
			std::string severity = match[3].str();
			std::string message = match[4].str();
			std::string error_code = match[5].str();

			int64_t line_number = 0;

			try {
				line_number = SafeParsing::SafeStoi(line_str);
			} catch (...) {
				// If parsing fails, keep as 0
			}

			ValidationEvent event;
			event.event_id = event_id++;

			// Map mypy severity to ValidationEventStatus
			if (severity == "error") {
				event.event_type = ValidationEventType::BUILD_ERROR;
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
			} else if (severity == "warning") {
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			} else if (severity == "note") {
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
			} else {
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			}

			event.message = message;
			event.ref_file = file_path;
			event.ref_line = line_number;
			event.ref_column = -1;
			event.error_code = error_code;
			event.function_name = ExtractMypyFunctionName(message);
			event.tool_name = "mypy";
			event.category = "type_checking";
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"error_code\": \"" + error_code + "\", \"severity\": \"" + severity + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for mypy message without error code
		else if (std::regex_search(line, match, RE_MYPY_MESSAGE_NO_CODE)) {
			std::string file_path = match[1].str();
			std::string line_str = match[2].str();
			std::string severity = match[3].str();
			std::string message = match[4].str();

			int64_t line_number = 0;

			try {
				line_number = SafeParsing::SafeStoi(line_str);
			} catch (...) {
				// If parsing fails, keep as 0
			}

			ValidationEvent event;
			event.event_id = event_id++;

			// Map mypy severity to ValidationEventStatus
			if (severity == "error") {
				event.event_type = ValidationEventType::BUILD_ERROR;
				event.severity = "error";
				event.status = ValidationEventStatus::ERROR;
			} else if (severity == "warning") {
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			} else if (severity == "note") {
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;
			} else {
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "warning";
				event.status = ValidationEventStatus::WARNING;
			}

			event.message = message;
			event.ref_file = file_path;
			event.ref_line = line_number;
			event.ref_column = -1;
			event.function_name = ExtractMypyFunctionName(message);
			event.tool_name = "mypy";
			event.category = "type_checking";
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"severity\": \"" + severity + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for summary with errors
		else if (std::regex_search(line, match, RE_MYPY_SUMMARY)) {
			std::string error_count = match[1].str();
			std::string file_count = match[2].str();
			std::string checked_count = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::ERROR;
			event.message =
			    "Found " + error_count + " errors in " + file_count + " files (checked " + checked_count + " files)";
			event.tool_name = "mypy";
			event.category = "type_checking";
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"error_count\": " + error_count + ", \"file_count\": " + file_count +
			                        ", \"checked_count\": " + checked_count + "}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for success message
		else if (std::regex_search(line, match, RE_MYPY_SUCCESS)) {
			std::string checked_count = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.status = ValidationEventStatus::PASS;
			event.message = "Success: no issues found in " + checked_count + " source files";
			event.tool_name = "mypy";
			event.category = "type_checking";
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"checked_count\": " + checked_count + "}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
