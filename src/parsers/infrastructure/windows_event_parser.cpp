#include "windows_event_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>
#include <unordered_map>

namespace duckdb {

// Windows Event Log export format (from Event Viewer or wevtutil):
// Log Name:      Security
// Source:        Microsoft-Windows-Security-Auditing
// Date:          1/15/2025 10:30:45 AM
// Event ID:      4624
// Task Category: Logon
// Level:         Information
// Keywords:      Audit Success
// User:          DOMAIN\username
// Computer:      HOSTNAME
// Description:
// An account was successfully logged on.

static std::string MapWindowsLevel(const std::string &level) {
	std::string lower = level;
	for (char &c : lower)
		c = std::tolower(c);

	if (lower == "error" || lower == "critical") {
		return "error";
	} else if (lower == "warning") {
		return "warning";
	}
	return "info"; // Information, Verbose
}

static ValidationEventStatus MapLevelToStatus(const std::string &severity) {
	if (severity == "error") {
		return ValidationEventStatus::ERROR;
	} else if (severity == "warning") {
		return ValidationEventStatus::WARNING;
	}
	return ValidationEventStatus::INFO;
}

// Parse multi-line Windows Event format
struct WindowsEventRecord {
	std::string log_name;
	std::string source;
	std::string date;
	std::string event_id;
	std::string task_category;
	std::string level;
	std::string keywords;
	std::string user;
	std::string computer;
	std::string description;
	int start_line;
	int end_line;
};

static bool ParseEventRecord(const WindowsEventRecord &record, ValidationEvent &event, int64_t event_id) {
	if (record.event_id.empty() && record.source.empty()) {
		return false;
	}

	event.event_id = event_id;
	event.tool_name = "windows_event";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = record.start_line;
	event.log_line_end = record.end_line;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	event.severity = MapWindowsLevel(record.level);
	event.status = MapLevelToStatus(event.severity);

	event.started_at = record.date;
	event.principal = record.user;
	event.category = record.log_name + "/" + record.source;
	event.error_code = record.event_id;

	// Build message
	if (!record.description.empty()) {
		event.message = record.description;
	} else {
		event.message = "Event " + record.event_id + " from " + record.source;
	}

	// Build structured_data JSON
	std::string json = "{";
	if (!record.log_name.empty())
		json += "\"log_name\":\"" + record.log_name + "\"";
	if (!record.source.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"source\":\"" + record.source + "\"";
	}
	if (!record.event_id.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"event_id\":\"" + record.event_id + "\"";
	}
	if (!record.task_category.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"task_category\":\"" + record.task_category + "\"";
	}
	if (!record.level.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"level\":\"" + record.level + "\"";
	}
	if (!record.keywords.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"keywords\":\"" + record.keywords + "\"";
	}
	if (!record.user.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"user\":\"" + record.user + "\"";
	}
	if (!record.computer.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"computer\":\"" + record.computer + "\"";
	}
	json += "}";
	event.structured_data = json;

	return true;
}

bool WindowsEventParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	// Look for Windows Event Log markers
	bool has_log_name = content.find("Log Name:") != std::string::npos;
	bool has_event_id = content.find("Event ID:") != std::string::npos;
	bool has_source = content.find("Source:") != std::string::npos;

	return (has_log_name || has_event_id) && has_source;
}

std::vector<ValidationEvent> WindowsEventParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int line_number = 0;

	WindowsEventRecord current_record;
	bool in_record = false;
	bool in_description = false;
	std::string raw_output;

	auto finalize_record = [&]() {
		if (in_record) {
			ValidationEvent event;
			event.log_content = raw_output;
			if (ParseEventRecord(current_record, event, event_id)) {
				events.push_back(event);
				event_id++;
			}
		}
		current_record = WindowsEventRecord();
		in_record = false;
		in_description = false;
		raw_output = "";
	};

	while (std::getline(stream, line)) {
		line_number++;

		// Trim trailing whitespace
		size_t end = line.find_last_not_of(" \t\r\n");
		std::string trimmed = (end != std::string::npos) ? line.substr(0, end + 1) : "";

		// Check for new record start
		if (trimmed.find("Log Name:") == 0) {
			finalize_record();
			in_record = true;
			current_record.start_line = line_number;
			current_record.log_name = trimmed.substr(9);
			// Trim leading whitespace
			size_t start = current_record.log_name.find_first_not_of(" \t");
			if (start != std::string::npos) {
				current_record.log_name = current_record.log_name.substr(start);
			}
			raw_output = line;
			continue;
		}

		if (!in_record)
			continue;

		raw_output += "\n" + line;
		current_record.end_line = line_number;

		// Parse known fields
		if (trimmed.find("Source:") == 0) {
			in_description = false;
			current_record.source = trimmed.substr(7);
			size_t start = current_record.source.find_first_not_of(" \t");
			if (start != std::string::npos)
				current_record.source = current_record.source.substr(start);
		} else if (trimmed.find("Date:") == 0) {
			in_description = false;
			current_record.date = trimmed.substr(5);
			size_t start = current_record.date.find_first_not_of(" \t");
			if (start != std::string::npos)
				current_record.date = current_record.date.substr(start);
		} else if (trimmed.find("Event ID:") == 0) {
			in_description = false;
			current_record.event_id = trimmed.substr(9);
			size_t start = current_record.event_id.find_first_not_of(" \t");
			if (start != std::string::npos)
				current_record.event_id = current_record.event_id.substr(start);
		} else if (trimmed.find("Task Category:") == 0) {
			in_description = false;
			current_record.task_category = trimmed.substr(14);
			size_t start = current_record.task_category.find_first_not_of(" \t");
			if (start != std::string::npos)
				current_record.task_category = current_record.task_category.substr(start);
		} else if (trimmed.find("Level:") == 0) {
			in_description = false;
			current_record.level = trimmed.substr(6);
			size_t start = current_record.level.find_first_not_of(" \t");
			if (start != std::string::npos)
				current_record.level = current_record.level.substr(start);
		} else if (trimmed.find("Keywords:") == 0) {
			in_description = false;
			current_record.keywords = trimmed.substr(9);
			size_t start = current_record.keywords.find_first_not_of(" \t");
			if (start != std::string::npos)
				current_record.keywords = current_record.keywords.substr(start);
		} else if (trimmed.find("User:") == 0) {
			in_description = false;
			current_record.user = trimmed.substr(5);
			size_t start = current_record.user.find_first_not_of(" \t");
			if (start != std::string::npos)
				current_record.user = current_record.user.substr(start);
		} else if (trimmed.find("Computer:") == 0) {
			in_description = false;
			current_record.computer = trimmed.substr(9);
			size_t start = current_record.computer.find_first_not_of(" \t");
			if (start != std::string::npos)
				current_record.computer = current_record.computer.substr(start);
		} else if (trimmed.find("Description:") == 0) {
			in_description = true;
			std::string desc = trimmed.substr(12);
			size_t start = desc.find_first_not_of(" \t");
			if (start != std::string::npos) {
				current_record.description = desc.substr(start);
			}
		} else if (in_description && !trimmed.empty()) {
			// Continuation of description
			if (!current_record.description.empty()) {
				current_record.description += " ";
			}
			current_record.description += trimmed;
		}
	}

	// Finalize last record
	finalize_record();

	return events;
}

} // namespace duckdb
