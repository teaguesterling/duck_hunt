#include "aws_cloudtrail_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Simple JSON value extraction (without full JSON parser dependency)
static std::string ExtractJSONString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t key_pos = json.find(search);
    if (key_pos == std::string::npos) return "";

    size_t colon_pos = json.find(':', key_pos + search.length());
    if (colon_pos == std::string::npos) return "";

    // Skip whitespace
    size_t start = json.find_first_not_of(" \t\n\r", colon_pos + 1);
    if (start == std::string::npos) return "";

    if (json[start] == '"') {
        // String value
        size_t end = start + 1;
        while (end < json.length()) {
            if (json[end] == '"' && json[end - 1] != '\\') break;
            end++;
        }
        return json.substr(start + 1, end - start - 1);
    } else if (json[start] == '{' || json[start] == '[') {
        // Object or array - skip for now
        return "";
    } else {
        // Number, bool, or null
        size_t end = json.find_first_of(",}\n\r", start);
        if (end == std::string::npos) end = json.length();
        std::string value = json.substr(start, end - start);
        // Trim whitespace
        size_t val_end = value.find_last_not_of(" \t\n\r");
        if (val_end != std::string::npos) {
            value = value.substr(0, val_end + 1);
        }
        return value;
    }
}

// Extract nested JSON string (e.g., userIdentity.userName)
static std::string ExtractNestedJSONString(const std::string& json, const std::string& parent_key, const std::string& child_key) {
    std::string search = "\"" + parent_key + "\"";
    size_t key_pos = json.find(search);
    if (key_pos == std::string::npos) return "";

    size_t colon_pos = json.find(':', key_pos + search.length());
    if (colon_pos == std::string::npos) return "";

    size_t start = json.find('{', colon_pos);
    if (start == std::string::npos) return "";

    // Find matching closing brace
    int depth = 1;
    size_t end = start + 1;
    while (end < json.length() && depth > 0) {
        if (json[end] == '{') depth++;
        else if (json[end] == '}') depth--;
        end++;
    }

    std::string nested = json.substr(start, end - start);
    return ExtractJSONString(nested, child_key);
}

static std::string MapEventNameToSeverity(const std::string& event_name, bool has_error) {
    if (has_error) {
        return "error";
    }
    // Security-sensitive events
    if (event_name.find("Delete") != std::string::npos ||
        event_name.find("Terminate") != std::string::npos ||
        event_name.find("Modify") != std::string::npos ||
        event_name.find("Update") != std::string::npos ||
        event_name.find("Put") != std::string::npos) {
        return "warning";
    }
    return "info";
}

static bool ParseCloudTrailRecord(const std::string& record, ValidationEvent& event, int64_t event_id, int line_number) {
    // Extract key fields
    std::string event_time = ExtractJSONString(record, "eventTime");
    std::string event_name = ExtractJSONString(record, "eventName");
    std::string event_source = ExtractJSONString(record, "eventSource");
    std::string aws_region = ExtractJSONString(record, "awsRegion");
    std::string source_ip = ExtractJSONString(record, "sourceIPAddress");
    std::string user_agent = ExtractJSONString(record, "userAgent");
    std::string error_code = ExtractJSONString(record, "errorCode");
    std::string error_message = ExtractJSONString(record, "errorMessage");
    std::string event_id_aws = ExtractJSONString(record, "eventID");

    // User identity fields
    std::string user_type = ExtractNestedJSONString(record, "userIdentity", "type");
    std::string user_name = ExtractNestedJSONString(record, "userIdentity", "userName");
    std::string user_arn = ExtractNestedJSONString(record, "userIdentity", "arn");
    std::string account_id = ExtractNestedJSONString(record, "userIdentity", "accountId");
    std::string principal_id = ExtractNestedJSONString(record, "userIdentity", "principalId");

    // Must have at least eventName to be valid CloudTrail
    if (event_name.empty()) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "aws_cloudtrail";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    // Field mappings - using new Phase 4 columns
    event.started_at = event_time;                 // Timestamp (proper column)
    event.function_name = event_name;              // API method name (e.g., CreateUser, DescribeInstances)
    event.category = event_source;                 // AWS service (e.g., ec2.amazonaws.com)
    event.message = event_name;                    // The action for display
    event.error_code = error_code;                 // AWS error code if present
    event.principal = user_arn;                    // User/service identity (ARN)
    event.origin = source_ip;                      // Network origin (caller IP)

    // Severity based on event type and error presence
    bool has_error = !error_code.empty();
    event.severity = MapEventNameToSeverity(event_name, has_error);
    if (has_error) {
        event.status = ValidationEventStatus::ERROR;
    } else if (event.severity == "warning") {
        event.status = ValidationEventStatus::WARNING;
    } else {
        event.status = ValidationEventStatus::INFO;
    }

    // Build structured_data JSON
    std::string json = "{";
    json += "\"aws_region\":\"" + aws_region + "\"";
    if (!source_ip.empty()) {
        json += ",\"source_ip\":\"" + source_ip + "\"";
    }
    if (!account_id.empty()) {
        json += ",\"account_id\":\"" + account_id + "\"";
    }
    if (!user_name.empty()) {
        json += ",\"user_name\":\"" + user_name + "\"";
    }
    if (!user_type.empty()) {
        json += ",\"user_type\":\"" + user_type + "\"";
    }
    if (!principal_id.empty()) {
        json += ",\"principal_id\":\"" + principal_id + "\"";
    }
    if (!event_id_aws.empty()) {
        json += ",\"event_id\":\"" + event_id_aws + "\"";
    }
    if (!user_agent.empty()) {
        // Escape quotes in user agent
        std::string escaped_ua;
        for (char c : user_agent) {
            if (c == '"') escaped_ua += "\\\"";
            else if (c == '\\') escaped_ua += "\\\\";
            else escaped_ua += c;
        }
        json += ",\"user_agent\":\"" + escaped_ua + "\"";
    }
    if (!error_message.empty()) {
        std::string escaped_msg;
        for (char c : error_message) {
            if (c == '"') escaped_msg += "\\\"";
            else if (c == '\\') escaped_msg += "\\\\";
            else escaped_msg += c;
        }
        json += ",\"error_message\":\"" + escaped_msg + "\"";
    }
    json += "}";
    event.structured_data = json;

    event.raw_output = record;
    return true;
}

bool AWSCloudTrailParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    // Look for CloudTrail-specific fields
    bool has_event_name = content.find("\"eventName\"") != std::string::npos;
    bool has_event_source = content.find("\"eventSource\"") != std::string::npos;
    bool has_aws_region = content.find("\"awsRegion\"") != std::string::npos;
    bool has_user_identity = content.find("\"userIdentity\"") != std::string::npos;

    // Need at least eventName and one other CloudTrail field
    return has_event_name && (has_event_source || has_aws_region || has_user_identity);
}

std::vector<ValidationEvent> AWSCloudTrailParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    int64_t event_id = 1;

    // Check if this is a CloudTrail Records array format
    size_t records_pos = content.find("\"Records\"");
    if (records_pos != std::string::npos) {
        // Find the array start
        size_t array_start = content.find('[', records_pos);
        if (array_start != std::string::npos) {
            // Parse each record object in the array
            size_t pos = array_start + 1;
            int line_number = 1;

            while (pos < content.length()) {
                // Find start of next object
                size_t obj_start = content.find('{', pos);
                if (obj_start == std::string::npos) break;

                // Find matching closing brace
                int depth = 1;
                size_t obj_end = obj_start + 1;
                while (obj_end < content.length() && depth > 0) {
                    if (content[obj_end] == '{') depth++;
                    else if (content[obj_end] == '}') depth--;
                    obj_end++;
                }

                std::string record = content.substr(obj_start, obj_end - obj_start);

                ValidationEvent event;
                if (ParseCloudTrailRecord(record, event, event_id, line_number)) {
                    events.push_back(event);
                    event_id++;
                }

                pos = obj_end;
                line_number++;
            }
        }
    } else {
        // Try parsing as JSONL (one record per line)
        std::istringstream stream(content);
        std::string line;
        int line_number = 0;

        while (std::getline(stream, line)) {
            line_number++;

            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            if (line.empty() || line[0] != '{') continue;

            ValidationEvent event;
            if (ParseCloudTrailRecord(line, event, event_id, line_number)) {
                events.push_back(event);
                event_id++;
            }
        }
    }

    return events;
}


} // namespace duckdb
