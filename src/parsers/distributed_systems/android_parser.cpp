#include "android_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

// Android log levels: V=Verbose, D=Debug, I=Info, W=Warning, E=Error, F=Fatal, S=Silent
static std::string MapAndroidLevel(char level) {
	switch (level) {
	case 'E':
	case 'F':
		return "error";
	case 'W':
		return "warning";
	default: // V, D, I, S
		return "info";
	}
}

static ValidationEventStatus MapLevelToStatus(const std::string &severity) {
	if (severity == "error") {
		return ValidationEventStatus::ERROR;
	} else if (severity == "warning") {
		return ValidationEventStatus::WARNING;
	}
	return ValidationEventStatus::INFO;
}

// Parse Android logcat line using string operations (no regex to avoid backtracking)
// Format: MM-DD HH:MM:SS.mmm  PID  TID LEVEL Tag: message
static bool ParseAndroidLine(const std::string &line, ValidationEvent &event, int64_t event_id, int line_number) {
	if (line.size() < 30)
		return false;

	size_t pos = 0;

	// Date: MM-DD
	if (!std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	std::string month = line.substr(pos, 2);
	pos += 2;
	if (line[pos] != '-')
		return false;
	pos++;
	if (!std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	std::string day = line.substr(pos, 2);
	pos += 2;

	// Space
	if (line[pos] != ' ')
		return false;
	pos++;

	// Time: HH:MM:SS.mmm
	if (pos + 12 > line.size())
		return false;
	size_t time_start = pos;
	// HH
	if (!std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	pos += 2;
	if (line[pos] != ':')
		return false;
	pos++;
	// MM
	if (!std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	pos += 2;
	if (line[pos] != ':')
		return false;
	pos++;
	// SS
	if (!std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]))
		return false;
	pos += 2;
	if (line[pos] != '.')
		return false;
	pos++;
	// mmm (milliseconds)
	if (!std::isdigit(line[pos]) || !std::isdigit(line[pos + 1]) || !std::isdigit(line[pos + 2]))
		return false;
	std::string time = line.substr(time_start, pos + 3 - time_start);
	pos += 3;

	// Skip spaces (there can be multiple spaces before PID)
	while (pos < line.size() && line[pos] == ' ') {
		pos++;
	}

	// PID: digits
	size_t pid_start = pos;
	while (pos < line.size() && std::isdigit(line[pos])) {
		pos++;
	}
	if (pos == pid_start)
		return false;
	std::string pid = line.substr(pid_start, pos - pid_start);

	// Skip spaces
	while (pos < line.size() && line[pos] == ' ') {
		pos++;
	}

	// TID: digits
	size_t tid_start = pos;
	while (pos < line.size() && std::isdigit(line[pos])) {
		pos++;
	}
	if (pos == tid_start)
		return false;
	std::string tid = line.substr(tid_start, pos - tid_start);

	// Skip space
	if (pos >= line.size() || line[pos] != ' ')
		return false;
	pos++;

	// Level: single character V/D/I/W/E/F/S
	if (pos >= line.size())
		return false;
	char level = line[pos];
	if (level != 'V' && level != 'D' && level != 'I' && level != 'W' && level != 'E' && level != 'F' && level != 'S') {
		return false;
	}
	pos++;

	// Space
	if (pos >= line.size() || line[pos] != ' ')
		return false;
	pos++;

	// Tag: until ':'
	size_t tag_start = pos;
	while (pos < line.size() && line[pos] != ':') {
		pos++;
	}
	if (pos == tag_start || pos >= line.size())
		return false;
	std::string tag = line.substr(tag_start, pos - tag_start);

	// Skip ':' and optional space
	pos++;
	if (pos < line.size() && line[pos] == ' ') {
		pos++;
	}

	// Message: rest of line
	std::string message = (pos < line.size()) ? line.substr(pos) : "";

	// Build timestamp (no year in Android logs)
	std::string timestamp = month + "-" + day + " " + time;

	event.event_id = event_id;
	event.tool_name = "android";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	event.started_at = timestamp;
	event.category = tag;
	event.message = message;
	event.severity = MapAndroidLevel(level);
	event.status = MapLevelToStatus(event.severity);

	// Build structured_data JSON
	event.structured_data = "{\"tag\":\"" + tag + "\",\"level\":\"" + std::string(1, level) + "\",\"pid\":\"" + pid +
	                        "\",\"tid\":\"" + tid + "\"}";
	event.log_content = line;

	return true;
}

bool AndroidParser::canParse(const std::string &content) const {
	if (content.empty())
		return false;

	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int android_lines = 0;
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

		// Check for Android logcat format: MM-DD HH:MM:SS.mmm PID TID LEVEL
		if (line.size() >= 25) {
			bool is_android = true;

			// Check MM-DD pattern
			if (!std::isdigit(line[0]) || !std::isdigit(line[1]))
				is_android = false;
			if (is_android && line[2] != '-')
				is_android = false;
			if (is_android && (!std::isdigit(line[3]) || !std::isdigit(line[4])))
				is_android = false;
			if (is_android && line[5] != ' ')
				is_android = false;

			// Check HH:MM:SS.mmm pattern
			if (is_android && (!std::isdigit(line[6]) || !std::isdigit(line[7])))
				is_android = false;
			if (is_android && line[8] != ':')
				is_android = false;
			if (is_android && (!std::isdigit(line[9]) || !std::isdigit(line[10])))
				is_android = false;
			if (is_android && line[11] != ':')
				is_android = false;
			if (is_android && (!std::isdigit(line[12]) || !std::isdigit(line[13])))
				is_android = false;
			if (is_android && line[14] != '.')
				is_android = false;

			// Look for single-letter log level followed by space and tag with colon
			if (is_android) {
				// After timestamp and PID/TID, look for " V ", " D ", " I ", " W ", " E ", " F "
				size_t level_pos = line.find(" V ", 18);
				if (level_pos == std::string::npos)
					level_pos = line.find(" D ", 18);
				if (level_pos == std::string::npos)
					level_pos = line.find(" I ", 18);
				if (level_pos == std::string::npos)
					level_pos = line.find(" W ", 18);
				if (level_pos == std::string::npos)
					level_pos = line.find(" E ", 18);
				if (level_pos == std::string::npos)
					level_pos = line.find(" F ", 18);

				if (level_pos != std::string::npos && line.find(':', level_pos) != std::string::npos) {
					android_lines++;
				}
			}
		}
	}

	return android_lines > 0 && android_lines >= (checked / 3);
}

std::vector<ValidationEvent> AndroidParser::parse(const std::string &content) const {
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
		if (ParseAndroidLine(line, event, event_id, line_number)) {
			events.push_back(event);
			event_id++;
		}
	}

	return events;
}

} // namespace duckdb
