#include "log4j_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Map Java log levels to our severity
static std::string MapJavaLevel(const std::string &level) {
	// Log4j/Logback levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
	if (level == "ERROR" || level == "FATAL" || level == "SEVERE") {
		return "error";
	} else if (level == "WARN" || level == "WARNING") {
		return "warning";
	}
	return "info"; // TRACE, DEBUG, INFO, FINE, FINER, FINEST
}

static ValidationEventStatus MapLevelToStatus(const std::string &severity) {
	if (severity == "error") {
		return ValidationEventStatus::ERROR;
	} else if (severity == "warning") {
		return ValidationEventStatus::WARNING;
	}
	return ValidationEventStatus::INFO;
}

// Parse Log4j/Logback text format:
// "2025-01-15 10:30:45,123 INFO  [main] com.example.App - Application started"
// "2025-01-15 10:30:46,456 ERROR [http-thread-1] com.example.Service - NullPointerException"
// Also handles variations like:
// "10:30:45.123 [main] INFO com.example.App - Message"
// "2025-01-15 10:30:45.123 | INFO | com.example.App | Message"
static bool ParseLog4jLine(const std::string &line, ValidationEvent &event, int64_t event_id, int line_number) {
	// Standard Log4j format: timestamp level [thread] logger - message
	static std::regex standard_pattern(
	    R"(^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}[,\.]\d+)\s+(TRACE|DEBUG|INFO|WARN|WARNING|ERROR|FATAL|SEVERE)\s+\[([^\]]+)\]\s+(\S+)\s+-\s+(.*)$)",
	    std::regex::optimize | std::regex::icase);

	// Logback default: timestamp [thread] level logger - message
	static std::regex logback_pattern(
	    R"(^(\d{2}:\d{2}:\d{2}[,\.]\d+)\s+\[([^\]]+)\]\s+(TRACE|DEBUG|INFO|WARN|WARNING|ERROR|FATAL)\s+(\S+)\s+-\s+(.*)$)",
	    std::regex::optimize | std::regex::icase);

	// Pipe-separated format: timestamp | level | logger | message
	static std::regex pipe_pattern(
	    R"(^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}[,\.]\d+)\s*\|\s*(TRACE|DEBUG|INFO|WARN|WARNING|ERROR|FATAL)\s*\|\s*(\S+)\s*\|\s*(.*)$)",
	    std::regex::optimize | std::regex::icase);

	// Simple format: timestamp level logger message (no thread)
	static std::regex simple_pattern(
	    R"(^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}[,\.]\d+)\s+(TRACE|DEBUG|INFO|WARN|WARNING|ERROR|FATAL|SEVERE)\s+(\S+)\s+-?\s*(.*)$)",
	    std::regex::optimize | std::regex::icase);

	std::smatch match;
	std::string timestamp;
	std::string level;
	std::string thread;
	std::string logger;
	std::string message;

	if (std::regex_match(line, match, standard_pattern)) {
		timestamp = match[1].str();
		level = match[2].str();
		thread = match[3].str();
		logger = match[4].str();
		message = match[5].str();
	} else if (std::regex_match(line, match, logback_pattern)) {
		timestamp = match[1].str();
		thread = match[2].str();
		level = match[3].str();
		logger = match[4].str();
		message = match[5].str();
	} else if (std::regex_match(line, match, pipe_pattern)) {
		timestamp = match[1].str();
		level = match[2].str();
		logger = match[3].str();
		message = match[4].str();
	} else if (std::regex_match(line, match, simple_pattern)) {
		timestamp = match[1].str();
		level = match[2].str();
		logger = match[3].str();
		message = match[4].str();
	} else {
		return false;
	}

	// Normalize level to uppercase
	std::string upper_level;
	for (char c : level) {
		upper_level += std::toupper(c);
	}

	event.event_id = event_id;
	event.tool_name = "log4j";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	// Field mappings
	event.started_at = timestamp;
	event.category = logger;
	event.message = message;
	event.severity = MapJavaLevel(upper_level);
	event.status = MapLevelToStatus(event.severity);

	// Build structured_data JSON
	std::string json = "{";
	json += "\"logger\":\"" + logger + "\"";
	json += ",\"level\":\"" + upper_level + "\"";
	if (!thread.empty()) {
		json += ",\"thread\":\"" + thread + "\"";
	}
	json += "}";
	event.structured_data = json;

	event.log_content = line;
	return true;
}

// Check for Java stack trace lines
static bool IsStackTraceLine(const std::string &line) {
	if (line.empty())
		return false;

	// Stack frame: "\tat com.example.Class.method(File.java:42)"
	if (line.find("\tat ") == 0 || line.find("    at ") == 0) {
		return true;
	}

	// Caused by line
	if (line.find("Caused by:") != std::string::npos) {
		return true;
	}

	// Suppressed exception
	if (line.find("Suppressed:") != std::string::npos) {
		return true;
	}

	// "... X more" line
	static std::regex more_pattern(R"(^\s*\.\.\.\s*\d+\s+more\s*$)");
	if (std::regex_match(line, more_pattern)) {
		return true;
	}

	return false;
}

// Check for exception header line
static bool IsExceptionLine(const std::string &line) {
	// Pattern: "com.example.SomeException: message"
	static std::regex exception_pattern(
	    R"(^([a-z][a-z0-9_]*\.)*[A-Z][a-zA-Z0-9_]*(Exception|Error|Throwable):\s*(.*)$)", std::regex::optimize);
	return std::regex_search(line, exception_pattern);
}

bool Log4jParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	std::istringstream stream(content);
	std::string line;
	int log4j_lines = 0;
	int checked = 0;

	// Pattern to detect Log4j-style logging
	static std::regex log4j_detect_pattern(
	    R"(^\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}[,\.]\d+\s+(TRACE|DEBUG|INFO|WARN|ERROR|FATAL))",
	    std::regex::optimize | std::regex::icase);
	static std::regex logback_detect_pattern(R"(^\d{2}:\d{2}:\d{2}[,\.]\d+\s+\[)", std::regex::optimize);

	while (std::getline(stream, line) && checked < 10) {
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);
		if (line.empty())
			continue;

		checked++;

		if (std::regex_search(line, log4j_detect_pattern) || std::regex_search(line, logback_detect_pattern)) {
			log4j_lines++;
		}
	}

	return log4j_lines > 0 && log4j_lines >= (checked / 3);
}

std::vector<ValidationEvent> Log4jParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int line_number = 0;

	ValidationEvent *current_event = nullptr;
	bool in_stacktrace = false;

	while (std::getline(stream, line)) {
		line_number++;

		// Preserve original line for raw_output but trim for parsing
		std::string trimmed = line;
		size_t end = trimmed.find_last_not_of("\r\n");
		if (end != std::string::npos) {
			trimmed = trimmed.substr(0, end + 1);
		}

		if (trimmed.empty()) {
			in_stacktrace = false;
			continue;
		}

		// Check if this is a stack trace continuation
		if (IsStackTraceLine(trimmed)) {
			if (current_event != nullptr) {
				current_event->log_content += "\n" + trimmed;
				current_event->log_line_end = line_number;

				// Extract file/line from stack frame
				// "\tat com.example.Class.method(File.java:42)"
				static std::regex stack_frame_pattern(
				    R"(\s*at\s+([a-zA-Z0-9_$.]+)\.([a-zA-Z0-9_$<>]+)\(([^:]+):(\d+)\))", std::regex::optimize);
				std::smatch match;
				if (std::regex_search(trimmed, match, stack_frame_pattern)) {
					// Only update if we don't have file info yet
					if (current_event->ref_file.empty()) {
						current_event->ref_file = match[3].str();
						try {
							current_event->ref_line = std::stoi(match[4].str());
						} catch (...) {
						}
						current_event->function_name = match[2].str();
					}
				}
			}
			in_stacktrace = true;
			continue;
		}

		// Check for exception line after a log entry
		if (IsExceptionLine(trimmed) && current_event != nullptr) {
			current_event->log_content += "\n" + trimmed;
			current_event->log_line_end = line_number;

			// Extract exception type and message
			static std::regex exception_extract(
			    R"(^((?:[a-z][a-z0-9_]*\.)*[A-Z][a-zA-Z0-9_]*(?:Exception|Error|Throwable)):\s*(.*)$)",
			    std::regex::optimize);
			std::smatch match;
			if (std::regex_match(trimmed, match, exception_extract)) {
				current_event->error_code = match[1].str();
				if (!match[2].str().empty()) {
					current_event->message = match[2].str();
				}
			}
			in_stacktrace = true;
			continue;
		}

		// Try to parse as a new log entry
		ValidationEvent event;
		if (ParseLog4jLine(trimmed, event, event_id, line_number)) {
			events.push_back(event);
			current_event = &events.back();
			event_id++;
			in_stacktrace = false;
		} else if (current_event != nullptr && !in_stacktrace) {
			// Continuation of previous message (multiline log)
			current_event->log_content += "\n" + trimmed;
			current_event->message += " " + trimmed;
			current_event->log_line_end = line_number;
		}
	}

	return events;
}

} // namespace duckdb
