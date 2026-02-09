#include "generic_error_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace duckdb {

// Helper to check if string starts with prefix (case-insensitive optional)
static bool StartsWithCI(const std::string &str, const std::string &prefix) {
	if (str.length() < prefix.length())
		return false;
	for (size_t i = 0; i < prefix.length(); i++) {
		if (std::tolower(str[i]) != std::tolower(prefix[i]))
			return false;
	}
	return true;
}

// Helper to trim leading whitespace
static std::string TrimLeft(const std::string &str) {
	size_t start = str.find_first_not_of(" \t");
	if (start == std::string::npos)
		return "";
	return str.substr(start);
}

int GenericErrorParser::classifyLine(const std::string &line, std::string &message) const {
	std::string trimmed = TrimLeft(line);
	if (trimmed.empty())
		return 0;

	// Error patterns (return 1)
	// Pattern: "error:" or "ERROR:" or "Error:"
	if (StartsWithCI(trimmed, "error:")) {
		message = TrimLeft(trimmed.substr(6));
		return 1;
	}
	if (StartsWithCI(trimmed, "error :")) {
		message = TrimLeft(trimmed.substr(7));
		return 1;
	}

	// Pattern: "[ERROR]" or "[error]"
	if (StartsWithCI(trimmed, "[error]")) {
		message = TrimLeft(trimmed.substr(7));
		return 1;
	}

	// Pattern: "[FAIL]" or "[fail]" or "[FAILED]" or "[failed]"
	if (StartsWithCI(trimmed, "[fail]")) {
		message = TrimLeft(trimmed.substr(6));
		return 1;
	}
	if (StartsWithCI(trimmed, "[failed]")) {
		message = TrimLeft(trimmed.substr(8));
		return 1;
	}

	// Pattern: "FAIL:" or "FAILED:"
	if (StartsWithCI(trimmed, "fail:")) {
		message = TrimLeft(trimmed.substr(5));
		return 1;
	}
	if (StartsWithCI(trimmed, "failed:")) {
		message = TrimLeft(trimmed.substr(7));
		return 1;
	}

	// Pattern: "fatal:" or "FATAL:" or "[FATAL]"
	if (StartsWithCI(trimmed, "fatal:")) {
		message = TrimLeft(trimmed.substr(6));
		return 1;
	}
	if (StartsWithCI(trimmed, "[fatal]")) {
		message = TrimLeft(trimmed.substr(7));
		return 1;
	}

	// Pattern: "critical:" or "[CRITICAL]"
	if (StartsWithCI(trimmed, "critical:")) {
		message = TrimLeft(trimmed.substr(9));
		return 1;
	}
	if (StartsWithCI(trimmed, "[critical]")) {
		message = TrimLeft(trimmed.substr(10));
		return 1;
	}

	// Pattern: "exception:" or "Exception:"
	if (StartsWithCI(trimmed, "exception:")) {
		message = TrimLeft(trimmed.substr(10));
		return 1;
	}

	// Warning patterns (return 2)
	// Pattern: "warning:" or "WARNING:" or "Warning:"
	if (StartsWithCI(trimmed, "warning:")) {
		message = TrimLeft(trimmed.substr(8));
		return 2;
	}
	if (StartsWithCI(trimmed, "warning :")) {
		message = TrimLeft(trimmed.substr(9));
		return 2;
	}

	// Pattern: "[WARNING]" or "[warning]" or "[WARN]" or "[warn]"
	if (StartsWithCI(trimmed, "[warning]")) {
		message = TrimLeft(trimmed.substr(9));
		return 2;
	}
	if (StartsWithCI(trimmed, "[warn]")) {
		message = TrimLeft(trimmed.substr(6));
		return 2;
	}

	// Pattern: "warn:" or "WARN:"
	if (StartsWithCI(trimmed, "warn:")) {
		message = TrimLeft(trimmed.substr(5));
		return 2;
	}

	// Pattern: "deprecated:" or "[DEPRECATED]"
	if (StartsWithCI(trimmed, "deprecated:")) {
		message = TrimLeft(trimmed.substr(11));
		return 2;
	}
	if (StartsWithCI(trimmed, "[deprecated]")) {
		message = TrimLeft(trimmed.substr(12));
		return 2;
	}

	// Info/notice patterns (return 3)
	// Pattern: "[INFO]" or "info:" or "[NOTICE]" or "notice:"
	if (StartsWithCI(trimmed, "[info]")) {
		message = TrimLeft(trimmed.substr(6));
		return 3;
	}
	if (StartsWithCI(trimmed, "info:")) {
		message = TrimLeft(trimmed.substr(5));
		return 3;
	}
	if (StartsWithCI(trimmed, "[notice]")) {
		message = TrimLeft(trimmed.substr(8));
		return 3;
	}
	if (StartsWithCI(trimmed, "notice:")) {
		message = TrimLeft(trimmed.substr(7));
		return 3;
	}

	return 0;
}

bool GenericErrorParser::canParse(const std::string &content) const {
	// Check if content has any error/warning patterns
	SafeParsing::SafeLineReader reader(content);
	std::string line;
	std::string message;
	int lines_checked = 0;
	int matches_found = 0;

	while (reader.getLine(line) && lines_checked < 100) {
		lines_checked++;
		if (classifyLine(line, message) > 0) {
			matches_found++;
			// Need at least one error/warning pattern to claim we can parse
			if (matches_found >= 1) {
				return true;
			}
		}
	}
	return false;
}

std::vector<ValidationEvent> GenericErrorParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	SafeParsing::SafeLineReader reader(content);
	std::string line;
	std::string message;
	int64_t event_id = 1;

	while (reader.getLine(line)) {
		int classification = classifyLine(line, message);
		if (classification == 0)
			continue;

		ValidationEvent event;
		event.event_id = event_id++;
		event.tool_name = "generic";
		event.event_type = ValidationEventType::LINT_ISSUE;
		event.ref_file = "";
		event.ref_line = -1;
		event.ref_column = -1;
		event.function_name = "";
		event.message = message.empty() ? line : message;
		event.execution_time = 0.0;
		event.log_content = line;
		event.log_line_start = reader.lineNumber();
		event.log_line_end = reader.lineNumber();

		switch (classification) {
		case 1: // Error
			event.status = ValidationEventStatus::ERROR;
			event.category = "generic_error";
			event.severity = "error";
			break;
		case 2: // Warning
			event.status = ValidationEventStatus::WARNING;
			event.category = "generic_warning";
			event.severity = "warning";
			break;
		case 3: // Info
			event.status = ValidationEventStatus::INFO;
			event.category = "generic_info";
			event.severity = "info";
			break;
		default:
			continue;
		}

		events.push_back(event);
	}

	return events;
}

} // namespace duckdb
