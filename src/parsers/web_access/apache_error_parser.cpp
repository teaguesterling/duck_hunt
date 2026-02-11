#include "apache_error_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

bool ApacheErrorParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int apache_error_lines = 0;
	int checked = 0;

	// Apache 2.2 error log: [Day Mon DD HH:MM:SS YYYY] [level] message
	// Apache 2.4 error log: [Day Mon DD HH:MM:SS.usec YYYY] [module:level] [pid N] message
	static std::regex apache_error_pattern(
	    R"(^\[(Mon|Tue|Wed|Thu|Fri|Sat|Sun)\s+(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?\s+\d{4}\]\s+\[)",
	    std::regex::optimize);

	while (reader.getLine(line) && checked < 10) {
		if (line.empty()) {
			continue;
		}
		checked++;

		std::smatch match;
		if (SafeParsing::SafeRegexSearch(line, match, apache_error_pattern)) {
			apache_error_lines++;
		}
	}

	// Need at least 3 matching lines out of first 10
	return apache_error_lines >= 3;
}

std::vector<ValidationEvent> ApacheErrorParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;

	// Apache 2.2 format: [Day Mon DD HH:MM:SS YYYY] [level] message
	static std::regex apache22_pattern(
	    R"(^\[(\w{3}\s+\w{3}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?\s+\d{4})\]\s+\[(\w+)\]\s+(.*)$)",
	    std::regex::optimize);

	// Apache 2.4 format: [Day Mon DD HH:MM:SS.usec YYYY] [module:level] [pid N] [client IP:port] message
	static std::regex apache24_pattern(
	    R"(^\[(\w{3}\s+\w{3}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?\s+\d{4})\]\s+\[([^:]+):(\w+)\](?:\s+\[pid\s+(\d+)\])?(?:\s+\[client\s+([^\]]+)\])?\s+(.*)$)",
	    std::regex::optimize);

	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int event_id = 0;

	while (reader.getLine(line)) {
		if (line.empty()) {
			continue;
		}

		std::smatch match;
		ValidationEvent event;
		event.tool_name = "apache_error";
		event.event_type = ValidationEventType::DEBUG_INFO;
		event.log_line_start = reader.lineNumber();
		event.log_line_end = reader.lineNumber();
		event.log_content = line;

		// Try Apache 2.4 format first (more specific)
		if (SafeParsing::SafeRegexMatch(line, match, apache24_pattern)) {
			event.started_at = match[1].str();
			std::string module = match[2].str();
			std::string level = match[3].str();
			std::string pid = match[4].str();
			std::string client = match[5].str();
			std::string message = match[6].str();

			event.category = module;
			event.message = message;
			if (!client.empty()) {
				event.origin = client;
			}

			// Map Apache log levels to severity
			if (level == "emerg" || level == "alert" || level == "crit" || level == "error") {
				event.severity = "error";
			} else if (level == "warn") {
				event.severity = "warning";
			} else {
				event.severity = "info";
			}
		}
		// Try Apache 2.2 format
		else if (SafeParsing::SafeRegexMatch(line, match, apache22_pattern)) {
			event.started_at = match[1].str();
			std::string level = match[2].str();
			std::string message = match[3].str();

			event.message = message;

			// Map Apache log levels to severity
			if (level == "emerg" || level == "alert" || level == "crit" || level == "error") {
				event.severity = "error";
			} else if (level == "warn") {
				event.severity = "warning";
			} else if (level == "notice") {
				event.severity = "info";
			} else {
				event.severity = "info"; // info, debug, trace
			}
			event.category = level;
		} else {
			// Line doesn't match expected format, skip
			continue;
		}

		event.event_id = ++event_id;
		events.push_back(event);
	}

	return events;
}

} // namespace duckdb
