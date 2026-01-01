#include "gcp_cloud_logging_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Simple JSON value extraction
static std::string ExtractJSONString(const std::string &json, const std::string &key) {
	std::string search = "\"" + key + "\"";
	size_t key_pos = json.find(search);
	if (key_pos == std::string::npos)
		return "";

	size_t colon_pos = json.find(':', key_pos + search.length());
	if (colon_pos == std::string::npos)
		return "";

	size_t start = json.find_first_not_of(" \t\n\r", colon_pos + 1);
	if (start == std::string::npos)
		return "";

	if (json[start] == '"') {
		size_t end = start + 1;
		while (end < json.length()) {
			if (json[end] == '"' && json[end - 1] != '\\')
				break;
			end++;
		}
		return json.substr(start + 1, end - start - 1);
	} else if (json[start] == '{' || json[start] == '[') {
		return "";
	} else {
		size_t end = json.find_first_of(",}\n\r", start);
		if (end == std::string::npos)
			end = json.length();
		std::string value = json.substr(start, end - start);
		size_t val_end = value.find_last_not_of(" \t\n\r");
		if (val_end != std::string::npos) {
			value = value.substr(0, val_end + 1);
		}
		return value;
	}
}

// Extract nested JSON string
static std::string ExtractNestedJSONString(const std::string &json, const std::string &parent_key,
                                           const std::string &child_key) {
	std::string search = "\"" + parent_key + "\"";
	size_t key_pos = json.find(search);
	if (key_pos == std::string::npos)
		return "";

	size_t colon_pos = json.find(':', key_pos + search.length());
	if (colon_pos == std::string::npos)
		return "";

	size_t start = json.find('{', colon_pos);
	if (start == std::string::npos)
		return "";

	int depth = 1;
	size_t end = start + 1;
	while (end < json.length() && depth > 0) {
		if (json[end] == '{')
			depth++;
		else if (json[end] == '}')
			depth--;
		end++;
	}

	std::string nested = json.substr(start, end - start);
	return ExtractJSONString(nested, child_key);
}

static std::string MapGCPSeverity(const std::string &severity) {
	// GCP severity levels: DEFAULT, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY
	if (severity == "ERROR" || severity == "CRITICAL" || severity == "ALERT" || severity == "EMERGENCY") {
		return "error";
	} else if (severity == "WARNING" || severity == "NOTICE") {
		return "warning";
	}
	return "info";
}

static bool ParseGCPLogEntry(const std::string &record, ValidationEvent &event, int64_t event_id, int line_number) {
	// Extract key fields
	std::string timestamp = ExtractJSONString(record, "timestamp");
	std::string severity = ExtractJSONString(record, "severity");
	std::string log_name = ExtractJSONString(record, "logName");
	std::string insert_id = ExtractJSONString(record, "insertId");
	std::string text_payload = ExtractJSONString(record, "textPayload");

	// Resource fields
	std::string resource_type = ExtractNestedJSONString(record, "resource", "type");
	std::string project_id = ExtractNestedJSONString(record, "resource", "project_id");
	std::string zone = ExtractNestedJSONString(record, "resource", "zone");

	// protoPayload fields (for audit logs)
	std::string method_name = ExtractNestedJSONString(record, "protoPayload", "methodName");
	std::string service_name = ExtractNestedJSONString(record, "protoPayload", "serviceName");
	std::string principal_email = ExtractNestedJSONString(record, "authenticationInfo", "principalEmail");
	std::string status_code = ExtractNestedJSONString(record, "status", "code");

	// Must have at least logName or severity to be valid GCP log
	if (log_name.empty() && severity.empty() && timestamp.empty()) {
		return false;
	}

	event.event_id = event_id;
	event.tool_name = "gcp_cloud_logging";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	// Field mappings - using new Phase 4 columns
	event.started_at = timestamp; // Timestamp (proper column)

	// function_name: method name for audit logs
	event.function_name = method_name;

	// Category: service name or resource type
	if (!service_name.empty()) {
		event.category = service_name;
	} else if (!resource_type.empty()) {
		event.category = resource_type;
	} else {
		event.category = "gcp";
	}

	// Message: method name (audit) or text payload (standard log)
	if (!method_name.empty()) {
		event.message = method_name;
	} else if (!text_payload.empty()) {
		event.message = text_payload;
	} else {
		event.message = log_name;
	}

	// principal: user/service identity
	event.principal = principal_email;

	// origin: GCP logs don't typically include caller IP in basic format
	// Could be extracted from requestMetadata.callerIp in protoPayload if needed
	// For now, leave empty

	// Error code from status
	event.error_code = status_code;

	// Severity mapping
	event.severity = MapGCPSeverity(severity);
	if (event.severity == "error") {
		event.status = ValidationEventStatus::ERROR;
	} else if (event.severity == "warning") {
		event.status = ValidationEventStatus::WARNING;
	} else {
		event.status = ValidationEventStatus::INFO;
	}

	// Build structured_data JSON
	std::string json = "{";
	bool first = true;

	auto add_field = [&json, &first](const std::string &key, const std::string &value) {
		if (!value.empty()) {
			if (!first)
				json += ",";
			first = false;
			// Escape value
			std::string escaped;
			for (char c : value) {
				if (c == '"')
					escaped += "\\\"";
				else if (c == '\\')
					escaped += "\\\\";
				else
					escaped += c;
			}
			json += "\"" + key + "\":\"" + escaped + "\"";
		}
	};

	add_field("log_name", log_name);
	add_field("insert_id", insert_id);
	add_field("resource_type", resource_type);
	add_field("project_id", project_id);
	add_field("zone", zone);
	add_field("severity", severity);
	add_field("principal_email", principal_email);
	add_field("service_name", service_name);

	json += "}";
	event.structured_data = json;

	event.log_content = record;
	return true;
}

bool GCPCloudLoggingParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	// Look for GCP-specific fields
	bool has_log_name = content.find("\"logName\"") != std::string::npos;
	bool has_insert_id = content.find("\"insertId\"") != std::string::npos;
	bool has_proto_payload = content.find("\"protoPayload\"") != std::string::npos;
	bool has_text_payload = content.find("\"textPayload\"") != std::string::npos;
	bool has_resource =
	    content.find("\"resource\"") != std::string::npos && content.find("\"type\"") != std::string::npos;

	// Need logName or insertId plus one other GCP field
	return (has_log_name || has_insert_id) && (has_proto_payload || has_text_payload || has_resource);
}

std::vector<ValidationEvent> GCPCloudLoggingParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	int64_t event_id = 1;

	// Check if this is an entries array format (from logging API)
	size_t entries_pos = content.find("\"entries\"");
	if (entries_pos != std::string::npos) {
		size_t array_start = content.find('[', entries_pos);
		if (array_start != std::string::npos) {
			size_t pos = array_start + 1;
			int line_number = 1;

			while (pos < content.length()) {
				size_t obj_start = content.find('{', pos);
				if (obj_start == std::string::npos)
					break;

				int depth = 1;
				size_t obj_end = obj_start + 1;
				while (obj_end < content.length() && depth > 0) {
					if (content[obj_end] == '{')
						depth++;
					else if (content[obj_end] == '}')
						depth--;
					obj_end++;
				}

				std::string record = content.substr(obj_start, obj_end - obj_start);

				ValidationEvent event;
				if (ParseGCPLogEntry(record, event, event_id, line_number)) {
					events.push_back(event);
					event_id++;
				}

				pos = obj_end;
				line_number++;
			}
		}
	} else {
		// Try parsing as JSONL
		std::istringstream stream(content);
		std::string line;
		int line_number = 0;

		while (std::getline(stream, line)) {
			line_number++;

			size_t start = line.find_first_not_of(" \t\r\n");
			if (start == std::string::npos)
				continue;
			line = line.substr(start);

			if (line.empty() || line[0] != '{')
				continue;

			ValidationEvent event;
			if (ParseGCPLogEntry(line, event, event_id, line_number)) {
				events.push_back(event);
				event_id++;
			}
		}
	}

	return events;
}

} // namespace duckdb
