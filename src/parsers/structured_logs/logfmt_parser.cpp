#include "logfmt_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <unordered_map>
#include <regex>

namespace duckdb {

// Pre-compiled regex patterns for logfmt parsing (compiled once, reused)
namespace {
static const std::regex RE_KV_PATTERN(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\s*=\s*(?:"[^"]*"|[^\s]*))");
} // anonymous namespace

// Helper functions (file-local)
static std::unordered_map<std::string, std::string> ParseLogfmtLine(const std::string &line) {
	std::unordered_map<std::string, std::string> fields;
	size_t pos = 0;
	size_t len = line.length();

	while (pos < len) {
		// Skip whitespace
		while (pos < len && (line[pos] == ' ' || line[pos] == '\t')) {
			pos++;
		}
		if (pos >= len)
			break;

		// Parse key
		size_t key_start = pos;
		while (pos < len && line[pos] != '=' && line[pos] != ' ' && line[pos] != '\t') {
			pos++;
		}

		if (pos >= len || line[pos] != '=') {
			while (pos < len && line[pos] != ' ' && line[pos] != '\t') {
				pos++;
			}
			continue;
		}

		std::string key = line.substr(key_start, pos - key_start);
		pos++; // Skip '='

		if (pos >= len) {
			fields[key] = "";
			break;
		}

		// Parse value
		std::string value;
		if (line[pos] == '"') {
			pos++; // Skip opening quote
			size_t value_start = pos;
			while (pos < len && line[pos] != '"') {
				if (line[pos] == '\\' && pos + 1 < len) {
					pos++;
				}
				pos++;
			}
			value = line.substr(value_start, pos - value_start);
			if (pos < len)
				pos++; // Skip closing quote
		} else {
			size_t value_start = pos;
			while (pos < len && line[pos] != ' ' && line[pos] != '\t') {
				pos++;
			}
			value = line.substr(value_start, pos - value_start);
		}

		fields[key] = value;
	}

	return fields;
}

static std::string GetFieldValue(const std::unordered_map<std::string, std::string> &fields,
                                 const std::vector<std::string> &field_names) {
	for (const auto &name : field_names) {
		auto it = fields.find(name);
		if (it != fields.end() && !it->second.empty()) {
			return it->second;
		}
	}
	return "";
}

static ValidationEventStatus MapLevelToStatus(const std::string &level) {
	if (level == "error" || level == "err" || level == "fatal" || level == "critical" || level == "crit" ||
	    level == "panic" || level == "dpanic") {
		return ValidationEventStatus::ERROR;
	} else if (level == "warn" || level == "warning") {
		return ValidationEventStatus::WARNING;
	}
	return ValidationEventStatus::INFO;
}

static std::string MapLevelToSeverity(const std::string &level) {
	if (level == "error" || level == "err" || level == "fatal" || level == "critical" || level == "crit" ||
	    level == "panic" || level == "dpanic") {
		return "error";
	} else if (level == "warn" || level == "warning") {
		return "warning";
	}
	return "info";
}

static ValidationEvent CreateEventFromFields(const std::unordered_map<std::string, std::string> &fields,
                                             int64_t event_id, int line_number, const std::string &raw_line) {
	ValidationEvent event;
	event.event_id = event_id;
	event.tool_name = "logfmt";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	// Extract level/severity
	std::string level = GetFieldValue(fields, {"level", "lvl", "severity", "loglevel"});
	if (!level.empty()) {
		level = StringUtil::Lower(level);
		event.status = MapLevelToStatus(level);
		event.severity = MapLevelToSeverity(level);
	} else {
		event.status = ValidationEventStatus::INFO;
		event.severity = "info";
	}

	// Extract message
	event.message = GetFieldValue(fields, {"msg", "message", "text"});

	// Extract timestamp
	std::string timestamp = GetFieldValue(fields, {"ts", "time", "timestamp", "t"});
	if (!timestamp.empty()) {
		event.function_name = timestamp;
	}

	// Extract error
	std::string error = GetFieldValue(fields, {"err", "error", "exception"});
	if (!error.empty()) {
		if (event.message.empty()) {
			event.message = error;
		} else {
			event.suggestion = error;
		}
		event.status = ValidationEventStatus::ERROR;
		event.severity = "error";
	}

	// Extract caller/source
	event.ref_file = GetFieldValue(fields, {"caller", "source", "file", "src"});

	// Extract component/logger
	event.category = GetFieldValue(fields, {"component", "logger", "service", "name", "module"});
	if (event.category.empty()) {
		event.category = "log_entry";
	}

	event.log_content = raw_line;
	return event;
}

bool LogfmtParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	std::istringstream stream(content);
	std::string line;
	int logfmt_lines = 0;
	int checked = 0;

	while (std::getline(stream, line) && checked < 5) {
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);
		if (line.empty())
			continue;

		// Skip if it looks like JSON
		if (line[0] == '{' || line[0] == '[') {
			return false;
		}

		checked++;

		// Pattern to match key=value or key="quoted value"
		auto begin = std::sregex_iterator(line.begin(), line.end(), RE_KV_PATTERN);
		auto end = std::sregex_iterator();
		int match_count = std::distance(begin, end);

		if (match_count >= 2) {
			logfmt_lines++;
		}
	}

	return logfmt_lines > 0 && logfmt_lines >= (checked / 2);
}

std::vector<ValidationEvent> LogfmtParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int line_number = 0;

	while (std::getline(stream, line)) {
		line_number++;

		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);

		size_t end = line.find_last_not_of(" \t\r\n");
		if (end != std::string::npos) {
			line = line.substr(0, end + 1);
		}

		if (line.empty())
			continue;

		auto fields = ParseLogfmtLine(line);
		if (!fields.empty()) {
			ValidationEvent event = CreateEventFromFields(fields, event_id++, line_number, line);
			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
