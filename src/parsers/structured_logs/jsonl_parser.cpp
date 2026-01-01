#include "jsonl_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include "yyjson.hpp"
#include <sstream>

namespace duckdb {

using namespace duckdb_yyjson;

// Helper functions (file-local)
static std::string ExtractStringField(yyjson_val *obj, const std::vector<const char *> &field_names) {
	for (const char *name : field_names) {
		yyjson_val *val = yyjson_obj_get(obj, name);
		if (val && yyjson_is_str(val)) {
			return yyjson_get_str(val);
		}
	}
	return "";
}

static int ExtractIntField(yyjson_val *obj, const std::vector<const char *> &field_names, int default_val) {
	for (const char *name : field_names) {
		yyjson_val *val = yyjson_obj_get(obj, name);
		if (val && yyjson_is_num(val)) {
			return yyjson_get_int(val);
		}
	}
	return default_val;
}

static ValidationEventStatus MapLevelToStatus(const std::string &level) {
	if (level == "error" || level == "err" || level == "fatal" || level == "critical" || level == "crit") {
		return ValidationEventStatus::ERROR;
	} else if (level == "warn" || level == "warning") {
		return ValidationEventStatus::WARNING;
	}
	return ValidationEventStatus::INFO;
}

static std::string MapLevelToSeverity(const std::string &level) {
	if (level == "error" || level == "err" || level == "fatal" || level == "critical" || level == "crit") {
		return "error";
	} else if (level == "warn" || level == "warning") {
		return "warning";
	}
	return "info";
}

static ValidationEvent ParseJsonLine(const std::string &line, int64_t event_id, int line_number) {
	ValidationEvent event;
	event.event_id = event_id;
	event.tool_name = "jsonl";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
	if (!doc) {
		throw IOException("Failed to parse JSON line");
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!yyjson_is_obj(root)) {
		yyjson_doc_free(doc);
		throw IOException("JSON line is not an object");
	}

	// Extract level/severity
	std::string level = ExtractStringField(root, {"level", "severity", "lvl", "loglevel", "log_level", "@l"});
	if (!level.empty()) {
		level = StringUtil::Lower(level);
		event.status = MapLevelToStatus(level);
		event.severity = MapLevelToSeverity(level);
	} else {
		event.status = ValidationEventStatus::INFO;
		event.severity = "info";
	}

	// Extract message
	event.message = ExtractStringField(root, {"message", "msg", "text", "content", "@m", "@mt", "short_message"});

	// Extract timestamp (store in function_name)
	std::string timestamp = ExtractStringField(root, {"timestamp", "ts", "time", "@timestamp", "@t", "datetime"});
	if (!timestamp.empty()) {
		event.function_name = timestamp;
	}

	// Extract error details
	std::string error = ExtractStringField(root, {"error", "err", "exception", "stack", "stacktrace"});
	if (!error.empty()) {
		if (event.message.empty()) {
			event.message = error;
		} else {
			event.suggestion = error;
		}
		event.status = ValidationEventStatus::ERROR;
		event.severity = "error";
	}

	// Extract file path if present
	event.ref_file = ExtractStringField(root, {"file", "file_path", "filepath", "filename", "source", "caller"});

	// Extract line number if present
	event.ref_line = ExtractIntField(root, {"line", "line_number", "lineno", "lineNumber"}, -1);

	// Extract category/logger name
	event.category = ExtractStringField(root, {"logger", "name", "category", "component", "service", "module"});
	if (event.category.empty()) {
		event.category = "log_entry";
	}

	// Store the raw JSON line
	event.log_content = line;

	yyjson_doc_free(doc);
	return event;
}

bool JSONLParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	// JSONL files should have lines starting with '{'
	std::istringstream stream(content);
	std::string line;
	int json_lines = 0;
	int checked = 0;

	while (std::getline(stream, line) && checked < 5) {
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;

		line = line.substr(start);
		if (line.empty())
			continue;

		checked++;

		if (line[0] != '{') {
			return false;
		}

		yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
		if (doc) {
			yyjson_val *root = yyjson_doc_get_root(doc);
			if (yyjson_is_obj(root)) {
				json_lines++;
			}
			yyjson_doc_free(doc);
		}
	}

	return json_lines > 0 && json_lines >= (checked / 2);
}

std::vector<ValidationEvent> JSONLParser::parse(const std::string &content) const {
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

		if (line.empty() || line[0] != '{')
			continue;

		try {
			ValidationEvent event = ParseJsonLine(line, event_id++, line_number);
			events.push_back(event);
		} catch (...) {
			continue;
		}
	}

	return events;
}

} // namespace duckdb
