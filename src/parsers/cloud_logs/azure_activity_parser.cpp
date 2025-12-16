#include "azure_activity_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Simple JSON value extraction
static std::string ExtractJSONString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t key_pos = json.find(search);
    if (key_pos == std::string::npos) return "";

    size_t colon_pos = json.find(':', key_pos + search.length());
    if (colon_pos == std::string::npos) return "";

    size_t start = json.find_first_not_of(" \t\n\r", colon_pos + 1);
    if (start == std::string::npos) return "";

    if (json[start] == '"') {
        size_t end = start + 1;
        while (end < json.length()) {
            if (json[end] == '"' && json[end - 1] != '\\') break;
            end++;
        }
        return json.substr(start + 1, end - start - 1);
    } else if (json[start] == '{' || json[start] == '[') {
        return "";
    } else {
        size_t end = json.find_first_of(",}\n\r", start);
        if (end == std::string::npos) end = json.length();
        std::string value = json.substr(start, end - start);
        size_t val_end = value.find_last_not_of(" \t\n\r");
        if (val_end != std::string::npos) {
            value = value.substr(0, val_end + 1);
        }
        return value;
    }
}

// Extract nested JSON string
static std::string ExtractNestedJSONString(const std::string& json, const std::string& parent_key, const std::string& child_key) {
    std::string search = "\"" + parent_key + "\"";
    size_t key_pos = json.find(search);
    if (key_pos == std::string::npos) return "";

    size_t colon_pos = json.find(':', key_pos + search.length());
    if (colon_pos == std::string::npos) return "";

    size_t start = json.find('{', colon_pos);
    if (start == std::string::npos) return "";

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

static std::string MapAzureLevel(const std::string& level, const std::string& status) {
    // Azure levels: Critical, Error, Warning, Informational, Verbose
    if (level == "Critical" || level == "Error") {
        return "error";
    } else if (level == "Warning") {
        return "warning";
    }
    // Also check status for failures
    if (status == "Failed" || status == "Failure") {
        return "error";
    }
    return "info";
}

static bool ParseAzureActivityEntry(const std::string& record, ValidationEvent& event, int64_t event_id, int line_number) {
    // Extract key fields
    std::string timestamp = ExtractJSONString(record, "time");
    if (timestamp.empty()) {
        timestamp = ExtractJSONString(record, "eventTimestamp");
    }
    std::string operation_name = ExtractJSONString(record, "operationName");
    std::string status = ExtractJSONString(record, "status");
    if (status.empty()) {
        status = ExtractNestedJSONString(record, "status", "value");
    }
    std::string caller = ExtractJSONString(record, "caller");
    std::string caller_ip = ExtractJSONString(record, "callerIpAddress");
    std::string category = ExtractJSONString(record, "category");
    std::string level = ExtractJSONString(record, "level");
    std::string resource_id = ExtractJSONString(record, "resourceId");
    std::string correlation_id = ExtractJSONString(record, "correlationId");
    std::string subscription_id = ExtractJSONString(record, "subscriptionId");
    std::string resource_group = ExtractJSONString(record, "resourceGroupName");
    std::string resource_provider = ExtractJSONString(record, "resourceProviderName");
    if (resource_provider.empty()) {
        resource_provider = ExtractNestedJSONString(record, "resourceProviderName", "value");
    }
    std::string result_type = ExtractJSONString(record, "resultType");
    std::string description = ExtractJSONString(record, "description");

    // Must have at least operationName to be valid Azure Activity Log
    if (operation_name.empty() && resource_id.empty()) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "azure_activity";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    // Field mappings - using new Phase 4 columns
    event.started_at = timestamp;                  // Timestamp (proper column)
    event.function_name = operation_name;          // Operation/method name

    // Category: Azure category or resource provider
    if (!category.empty()) {
        event.category = category;
    } else if (!resource_provider.empty()) {
        event.category = resource_provider;
    } else {
        event.category = "azure";
    }

    // Message: operation name
    event.message = operation_name;

    // principal: caller identity (email or service principal)
    event.principal = caller;

    // origin: caller IP address
    event.origin = caller_ip;

    // Error code from status or result type
    if (!status.empty()) {
        event.error_code = status;
    } else {
        event.error_code = result_type;
    }

    // Severity mapping
    event.severity = MapAzureLevel(level, status);
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

    auto add_field = [&json, &first](const std::string& key, const std::string& value) {
        if (!value.empty()) {
            if (!first) json += ",";
            first = false;
            // Escape value
            std::string escaped;
            for (char c : value) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else escaped += c;
            }
            json += "\"" + key + "\":\"" + escaped + "\"";
        }
    };

    add_field("resource_id", resource_id);
    add_field("subscription_id", subscription_id);
    add_field("resource_group", resource_group);
    add_field("resource_provider", resource_provider);
    add_field("caller_ip", caller_ip);
    add_field("correlation_id", correlation_id);
    add_field("level", level);
    add_field("description", description);

    json += "}";
    event.structured_data = json;

    event.raw_output = record;
    return true;
}

bool AzureActivityParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    // Look for Azure-specific fields
    bool has_operation_name = content.find("\"operationName\"") != std::string::npos;
    bool has_resource_id = content.find("\"resourceId\"") != std::string::npos;
    bool has_caller = content.find("\"caller\"") != std::string::npos;
    bool has_subscription_id = content.find("\"subscriptionId\"") != std::string::npos;
    bool has_correlation_id = content.find("\"correlationId\"") != std::string::npos;

    // Need operationName plus one other Azure-specific field
    return has_operation_name && (has_resource_id || has_caller || has_subscription_id || has_correlation_id);
}

std::vector<ValidationEvent> AzureActivityParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    int64_t event_id = 1;

    // Check if this is a records/value array format
    size_t records_pos = content.find("\"records\"");
    if (records_pos == std::string::npos) {
        records_pos = content.find("\"value\"");
    }

    if (records_pos != std::string::npos) {
        size_t array_start = content.find('[', records_pos);
        if (array_start != std::string::npos) {
            size_t pos = array_start + 1;
            int line_number = 1;

            while (pos < content.length()) {
                size_t obj_start = content.find('{', pos);
                if (obj_start == std::string::npos) break;

                int depth = 1;
                size_t obj_end = obj_start + 1;
                while (obj_end < content.length() && depth > 0) {
                    if (content[obj_end] == '{') depth++;
                    else if (content[obj_end] == '}') depth--;
                    obj_end++;
                }

                std::string record = content.substr(obj_start, obj_end - obj_start);

                ValidationEvent event;
                if (ParseAzureActivityEntry(record, event, event_id, line_number)) {
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
            if (start == std::string::npos) continue;
            line = line.substr(start);

            if (line.empty() || line[0] != '{') continue;

            ValidationEvent event;
            if (ParseAzureActivityEntry(line, event, event_id, line_number)) {
                events.push_back(event);
                event_id++;
            }
        }
    }

    return events;
}


} // namespace duckdb
