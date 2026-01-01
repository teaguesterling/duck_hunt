#include "spark_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

static std::string MapSparkLevel(const std::string &level) {
	if (level == "ERROR" || level == "FATAL" || level == "SEVERE") {
		return "error";
	} else if (level == "WARN" || level == "WARNING") {
		return "warning";
	}
	return "info";
}

static ValidationEventStatus MapLevelToStatus(const std::string &severity) {
	if (severity == "error") {
		return ValidationEventStatus::ERROR;
	} else if (severity == "warning") {
		return ValidationEventStatus::WARNING;
	}
	return ValidationEventStatus::INFO;
}

// Parse Spark log line using string operations (no regex to avoid backtracking)
// Format: YY/MM/DD HH:MM:SS LEVEL component: message
static bool ParseSparkLine(const std::string &line, ValidationEvent &event, int64_t event_id, int line_number) {
	if (line.size() < 25)
		return false;

	size_t pos = 0;

	// Date: YY/MM/DD
	// YY
	if (pos + 2 >= line.size() || !std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	std::string year = line.substr(pos, 2);
	pos += 2;
	if (line[pos] != '/')
		return false;
	pos++;

	// MM
	if (pos + 2 >= line.size() || !std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	std::string month = line.substr(pos, 2);
	pos += 2;
	if (line[pos] != '/')
		return false;
	pos++;

	// DD
	if (pos + 2 >= line.size() || !std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	std::string day = line.substr(pos, 2);
	pos += 2;

	// Space
	if (line[pos] != ' ')
		return false;
	pos++;

	// Time: HH:MM:SS
	if (pos + 8 > line.size())
		return false;
	if (!std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	if (line[pos + 2] != ':')
		return false;
	if (!std::isdigit(line[pos + 3]) || !std::isdigit(line[pos + 4]))
		return false;
	if (line[pos + 5] != ':')
		return false;
	if (!std::isdigit(line[pos + 6]) || !std::isdigit(line[pos + 7]))
		return false;
	std::string time = line.substr(pos, 8);
	pos += 8;

	// Space
	if (pos >= line.size() || line[pos] != ' ')
		return false;
	pos++;

	// Level: alphabetic word
	size_t level_start = pos;
	while (pos < line.size() && std::isalpha(line[pos])) {
		pos++;
	}
	if (pos == level_start)
		return false;
	std::string level = line.substr(level_start, pos - level_start);

	// Validate level
	if (level != "INFO" && level != "WARN" && level != "WARNING" && level != "ERROR" && level != "FATAL" &&
	    level != "DEBUG" && level != "TRACE") {
		return false;
	}

	// Space
	if (pos >= line.size() || line[pos] != ' ')
		return false;
	pos++;

	// Component: until ':' (or end of identifier characters)
	size_t comp_start = pos;
	while (pos < line.size() && line[pos] != ':' && line[pos] != ' ') {
		pos++;
	}
	if (pos == comp_start)
		return false;
	std::string component = line.substr(comp_start, pos - comp_start);

	// Skip ':' if present
	if (pos < line.size() && line[pos] == ':') {
		pos++;
	}

	// Skip optional space after colon
	if (pos < line.size() && line[pos] == ' ') {
		pos++;
	}

	// Message: rest of line
	std::string message = (pos < line.size()) ? line.substr(pos) : "";

	// Build timestamp: 20YY-MM-DD HH:MM:SS
	std::string timestamp = "20" + year + "-" + month + "-" + day + " " + time;

	event.event_id = event_id;
	event.tool_name = "spark";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	event.started_at = timestamp;
	event.category = component;
	event.message = message;
	event.severity = MapSparkLevel(level);
	event.status = MapLevelToStatus(event.severity);

	// Build structured_data JSON
	event.structured_data = "{\"component\":\"" + component + "\",\"level\":\"" + level + "\"}";
	event.log_content = line;

	return true;
}

bool SparkParser::canParse(const std::string &content) const {
	if (content.empty())
		return false;

	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int spark_lines = 0;
	int checked = 0;

	while (reader.getLine(line) && checked < 10) {
		// Skip empty lines
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);
		if (line.empty())
			continue;

		checked++;

		// Check for Spark format: YY/MM/DD HH:MM:SS LEVEL
		if (line.size() >= 20) {
			bool is_spark = true;
			// Check YY/MM/DD pattern
			if (!std::isdigit(line[0]) || !std::isdigit(line[1]))
				is_spark = false;
			if (is_spark && line[2] != '/')
				is_spark = false;
			if (is_spark && (!std::isdigit(line[3]) || !std::isdigit(line[4])))
				is_spark = false;
			if (is_spark && line[5] != '/')
				is_spark = false;
			if (is_spark && (!std::isdigit(line[6]) || !std::isdigit(line[7])))
				is_spark = false;
			if (is_spark && line[8] != ' ')
				is_spark = false;

			// Check for INFO/WARN/ERROR after timestamp
			if (is_spark) {
				if (line.find(" INFO ", 17) != std::string::npos || line.find(" WARN ", 17) != std::string::npos ||
				    line.find(" ERROR ", 17) != std::string::npos || line.find(" DEBUG ", 17) != std::string::npos ||
				    line.find(" FATAL ", 17) != std::string::npos) {
					spark_lines++;
				}
			}
		}
	}

	return spark_lines > 0 && spark_lines >= (checked / 3);
}

std::vector<ValidationEvent> SparkParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int64_t event_id = 1;

	while (reader.getLine(line)) {
		int line_number = reader.lineNumber();

		// Remove trailing whitespace
		size_t end = line.find_last_not_of("\r\n");
		if (end != std::string::npos) {
			line = line.substr(0, end + 1);
		}

		if (line.empty())
			continue;

		ValidationEvent event;
		if (ParseSparkLine(line, event, event_id, line_number)) {
			events.push_back(event);
			event_id++;
		}
	}

	return events;
}

} // namespace duckdb
