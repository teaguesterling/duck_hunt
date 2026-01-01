#include "nlog_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// NLog levels
static std::string MapNLogLevel(const std::string &level) {
	std::string upper = level;
	for (char &c : upper)
		c = std::toupper(c);

	if (upper == "FATAL" || upper == "ERROR") {
		return "error";
	} else if (upper == "WARN") {
		return "warning";
	}
	return "info"; // TRACE, DEBUG, INFO
}

static ValidationEventStatus MapLevelToStatus(const std::string &severity) {
	if (severity == "error")
		return ValidationEventStatus::ERROR;
	if (severity == "warning")
		return ValidationEventStatus::WARNING;
	return ValidationEventStatus::INFO;
}

// Parse NLog text format:
// "2025-01-15 10:30:45.1234|INFO|MyApp.Program|Application started"
// "2025-01-15 10:30:46.5678|ERROR|MyApp.Service|Connection failed|System.TimeoutException"
static bool ParseNLogLine(const std::string &line, ValidationEvent &event, int64_t event_id, int line_number) {
	// NLog default format: timestamp|level|logger|message[|exception]
	static std::regex nlog_pattern(
	    R"(^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?)\|(\w+)\|([^|]+)\|([^|]*)(?:\|(.*))?$)",
	    std::regex::optimize);

	std::smatch match;
	if (!std::regex_match(line, match, nlog_pattern)) {
		return false;
	}

	event.event_id = event_id;
	event.tool_name = "nlog";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	event.started_at = match[1].str();
	std::string level = match[2].str();
	event.category = match[3].str(); // Logger name
	event.message = match[4].str();

	if (match[5].matched && !match[5].str().empty()) {
		event.error_code = match[5].str(); // Exception type
	}

	event.severity = MapNLogLevel(level);
	event.status = MapLevelToStatus(event.severity);

	// Build structured_data JSON
	std::string json = "{";
	json += "\"level\":\"" + level + "\"";
	json += ",\"logger\":\"" + event.category + "\"";
	if (!event.error_code.empty()) {
		json += ",\"exception\":\"" + event.error_code + "\"";
	}
	json += "}";
	event.structured_data = json;

	event.log_content = line;
	return true;
}

bool NLogParser::canParse(const std::string &content) const {
	if (content.empty())
		return false;

	std::istringstream stream(content);
	std::string line;
	int nlog_lines = 0;
	int checked = 0;

	// NLog detection: timestamp|LEVEL|logger|message
	static std::regex nlog_detect(
	    R"(^\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?\|(TRACE|DEBUG|INFO|WARN|ERROR|FATAL)\|)",
	    std::regex::optimize | std::regex::icase);

	while (std::getline(stream, line) && checked < 10) {
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);
		if (line.empty())
			continue;

		checked++;

		if (std::regex_search(line, nlog_detect)) {
			nlog_lines++;
		}
	}

	return nlog_lines > 0 && nlog_lines >= (checked / 3);
}

std::vector<ValidationEvent> NLogParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int line_number = 0;

	while (std::getline(stream, line)) {
		line_number++;

		size_t end = line.find_last_not_of(" \t\r\n");
		if (end != std::string::npos) {
			line = line.substr(0, end + 1);
		}

		if (line.empty())
			continue;

		ValidationEvent event;
		if (ParseNLogLine(line, event, event_id, line_number)) {
			events.push_back(event);
			event_id++;
		}
	}

	return events;
}

} // namespace duckdb
