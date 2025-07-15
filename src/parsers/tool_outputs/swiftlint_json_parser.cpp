#include "swiftlint_json_parser.hpp"
#include "../../core/parser_registry.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool SwiftLintJSONParser::canParse(const std::string& content) const {
    // Quick checks for SwiftLint JSON patterns
    if (content.find("\"file\"") != std::string::npos &&
        content.find("\"rule_id\"") != std::string::npos &&
        content.find("\"reason\"") != std::string::npos &&
        content.find("\"severity\"") != std::string::npos) {
        return isValidSwiftLintJSON(content);
    }
    return false;
}

bool SwiftLintJSONParser::isValidSwiftLintJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_arr(root)) {
        // Check if first element has SwiftLint structure
        size_t idx, max;
        yyjson_val *violation;
        yyjson_arr_foreach(root, idx, max, violation) {
            if (yyjson_is_obj(violation)) {
                yyjson_val *rule_id = yyjson_obj_get(violation, "rule_id");
                yyjson_val *reason = yyjson_obj_get(violation, "reason");
                yyjson_val *severity = yyjson_obj_get(violation, "severity");
                
                if (rule_id && yyjson_is_str(rule_id) &&
                    reason && yyjson_is_str(reason) &&
                    severity && yyjson_is_str(severity)) {
                    is_valid = true;
                    break;
                }
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> SwiftLintJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        return events; // Return empty instead of throwing
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        return events;
    }
    
    // Parse each violation
    size_t idx, max;
    yyjson_val *violation;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, violation) {
        if (!yyjson_is_obj(violation)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "swiftlint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.line_number = -1;
        event.column_number = -1;
        event.execution_time = 0.0;
        event.category = "code_quality";
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(violation, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line and column
        yyjson_val *line = yyjson_obj_get(violation, "line");
        if (line && yyjson_is_num(line)) {
            event.line_number = yyjson_get_int(line);
        }
        
        yyjson_val *column = yyjson_obj_get(violation, "column");
        if (column && yyjson_is_num(column)) {
            event.column_number = yyjson_get_int(column);
        }
        
        // Get severity
        yyjson_val *severity = yyjson_obj_get(violation, "severity");
        if (severity && yyjson_is_str(severity)) {
            std::string severity_str = yyjson_get_str(severity);
            if (severity_str == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else if (severity_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.severity = severity_str;
            }
        }
        
        // Get reason (message)
        yyjson_val *reason = yyjson_obj_get(violation, "reason");
        if (reason && yyjson_is_str(reason)) {
            event.message = yyjson_get_str(reason);
        }
        
        // Get rule ID
        yyjson_val *rule_id = yyjson_obj_get(violation, "rule_id");
        if (rule_id && yyjson_is_str(rule_id)) {
            event.error_code = yyjson_get_str(rule_id);
        }
        
        // Get type (rule type)
        yyjson_val *type = yyjson_obj_get(violation, "type");
        if (type && yyjson_is_str(type)) {
            event.suggestion = yyjson_get_str(type);
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "{\"tool\": \"swiftlint\", \"rule_id\": \"" + event.error_code + "\", \"type\": \"" + event.suggestion + "\"}";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
    return events;
}

// Auto-register this parser
REGISTER_PARSER(SwiftLintJSONParser);

} // namespace duckdb