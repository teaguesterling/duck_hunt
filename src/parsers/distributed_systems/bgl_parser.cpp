#include "bgl_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>
#include <vector>

namespace duckdb {

static std::string MapBglLevel(const std::string &level) {
	if (level == "FATAL" || level == "FAILURE" || level == "SEVERE" || level == "ERROR") {
		return "error";
	} else if (level == "WARNING" || level == "WARN") {
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

// Split string by spaces into tokens
static std::vector<std::string> SplitBySpace(const std::string &str, size_t max_tokens = 0) {
	std::vector<std::string> tokens;
	size_t start = 0;
	size_t end = 0;

	while (end != std::string::npos && (max_tokens == 0 || tokens.size() < max_tokens - 1)) {
		end = str.find(' ', start);
		std::string token;
		if (end == std::string::npos) {
			token = str.substr(start);
		} else {
			token = str.substr(start, end - start);
		}
		if (!token.empty()) {
			tokens.push_back(token);
		}
		start = end + 1;
	}

	// Add rest as last token if max_tokens specified
	if (max_tokens > 0 && end != std::string::npos && tokens.size() == max_tokens - 1) {
		// Skip any leading spaces
		while (start < str.size() && str[start] == ' ')
			start++;
		if (start < str.size()) {
			tokens.push_back(str.substr(start));
		}
	}

	return tokens;
}

// Parse BGL log line
// Format: ALERT_TYPE UNIX_TS DATE NODE TIMESTAMP NODE SOURCE COMPONENT LEVEL message
static bool ParseBglLine(const std::string &line, ValidationEvent &event, int64_t event_id, int line_number) {
	if (line.size() < 50)
		return false;

	// Split into tokens (need at least 9 fields + message)
	auto tokens = SplitBySpace(line, 10);
	if (tokens.size() < 9)
		return false;

	// Token 0: Alert type (- or APPREAD, APPERROR, etc.)
	std::string alert_type = tokens[0];

	// Token 1: Unix timestamp (should be all digits)
	std::string unix_ts = tokens[1];
	for (char c : unix_ts) {
		if (!std::isdigit(c))
			return false;
	}

	// Token 2: Date YYYY.MM.DD
	std::string date = tokens[2];
	if (date.size() != 10 || date[4] != '.' || date[7] != '.')
		return false;

	// Token 3: Node identifier (contains dashes and colons like R02-M1-N0-C:J12-U11)
	std::string node = tokens[3];
	if (node.find('-') == std::string::npos)
		return false;

	// Token 4: Full timestamp YYYY-MM-DD-HH.MM.SS.microseconds
	std::string full_timestamp = tokens[4];
	if (full_timestamp.size() < 19)
		return false;

	// Token 5: Node again (should match token 3)
	// std::string node2 = tokens[5];

	// Token 6: Source (RAS, etc.)
	std::string source = tokens[6];

	// Token 7: Component (KERNEL, APP, etc.)
	std::string component = tokens[7];

	// Token 8: Level (INFO, FATAL, WARNING, etc.)
	std::string level = tokens[8];

	// Validate level is reasonable
	if (level != "INFO" && level != "FATAL" && level != "WARNING" && level != "FAILURE" && level != "ERROR" &&
	    level != "SEVERE" && level != "DEBUG") {
		return false;
	}

	// Token 9+: Message (rest of line)
	std::string message;
	if (tokens.size() > 9) {
		message = tokens[9];
	}

	// Convert timestamp from YYYY-MM-DD-HH.MM.SS to YYYY-MM-DD HH:MM:SS
	std::string timestamp = full_timestamp.substr(0, 10); // YYYY-MM-DD
	if (full_timestamp.size() >= 19) {
		timestamp += " ";
		timestamp += full_timestamp[11]; // H
		timestamp += full_timestamp[12]; // H
		timestamp += ":";
		timestamp += full_timestamp[14]; // M
		timestamp += full_timestamp[15]; // M
		timestamp += ":";
		timestamp += full_timestamp[17]; // S
		timestamp += full_timestamp[18]; // S
	}

	event.event_id = event_id;
	event.tool_name = "bgl";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	event.started_at = timestamp;
	event.category = source + "/" + component;
	event.message = message;
	event.severity = MapBglLevel(level);
	event.status = MapLevelToStatus(event.severity);

	// Build structured_data JSON
	event.structured_data = "{\"node\":\"" + node + "\",\"source\":\"" + source + "\",\"component\":\"" + component +
	                        "\",\"level\":\"" + level + "\",\"alert_type\":\"" + alert_type + "\"}";

	event.log_content = line;

	return true;
}

bool BglParser::canParse(const std::string &content) const {
	if (content.empty())
		return false;

	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int bgl_lines = 0;
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

		// BGL format indicators:
		// - Starts with '-' or 'APP' followed by space
		// - Contains node identifiers like R02-M1-N0-C:J12-U11
		// - Contains RAS KERNEL or RAS APP

		bool starts_correctly = false;
		if (line.size() >= 2 && line[0] == '-' && line[1] == ' ') {
			starts_correctly = true;
		} else if (line.size() >= 8) {
			if (line.compare(0, 4, "APP ") == 0 || line.compare(0, 7, "APPREAD") == 0 ||
			    line.compare(0, 8, "APPERROR") == 0 || line.compare(0, 7, "KERNELP") == 0) {
				starts_correctly = true;
			}
		}

		bool has_node = false;
		// Look for R##-M#-N#- pattern
		for (size_t i = 0; i + 10 < line.size(); i++) {
			if (line[i] == 'R' && std::isdigit(line[i + 1]) && std::isdigit(line[i + 2]) && line[i + 3] == '-' &&
			    line[i + 4] == 'M') {
				has_node = true;
				break;
			}
		}

		bool has_ras = (line.find(" RAS ") != std::string::npos);

		if (starts_correctly && has_node && has_ras) {
			bgl_lines++;
		}
	}

	return bgl_lines > 0 && bgl_lines >= (checked / 3);
}

std::vector<ValidationEvent> BglParser::parse(const std::string &content) const {
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
		if (ParseBglLine(line, event, event_id, line_number)) {
			events.push_back(event);
			event_id++;
		}
	}

	return events;
}

} // namespace duckdb
