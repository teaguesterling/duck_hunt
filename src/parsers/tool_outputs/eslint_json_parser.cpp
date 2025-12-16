#include "eslint_json_parser.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool ESLintJSONParser::canParse(const std::string& content) const {
    // Quick heuristic checks before parsing
    if (content.empty() || content[0] != '[') {
        return false;
    }
    
    // Look for ESLint-specific JSON structure indicators
    if (content.find("\"filePath\"") == std::string::npos ||
        content.find("\"messages\"") == std::string::npos) {
        return false;
    }
    
    return isValidESLintJSON(content);
}

bool ESLintJSONParser::isValidESLintJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        return false;
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_arr(root)) {
        // Check if first element has ESLint structure
        size_t arr_size = yyjson_arr_size(root);
        if (arr_size > 0) {
            yyjson_val *first_elem = yyjson_arr_get_first(root);
            if (yyjson_is_obj(first_elem)) {
                yyjson_val *file_path = yyjson_obj_get(first_elem, "filePath");
                yyjson_val *messages = yyjson_obj_get(first_elem, "messages");
                
                is_valid = (file_path && yyjson_is_str(file_path)) &&
                          (messages && yyjson_is_arr(messages));
            }
        } else {
            // Empty array is valid ESLint output
            is_valid = true;
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> ESLintJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse ESLint JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid ESLint JSON: root is not an array");
    }
    
    // Parse each file result
    size_t idx, max;
    yyjson_val *file_result;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, file_result) {
        if (!yyjson_is_obj(file_result)) continue;
        
        // Get file path
        yyjson_val *file_path = yyjson_obj_get(file_result, "filePath");
        std::string file_path_str;
        if (file_path && yyjson_is_str(file_path)) {
            file_path_str = yyjson_get_str(file_path);
        }
        
        // Get messages array
        yyjson_val *messages = yyjson_obj_get(file_result, "messages");
        if (!messages || !yyjson_is_arr(messages)) continue;
        
        // Parse each message
        size_t msg_idx, msg_max;
        yyjson_val *message;
        yyjson_arr_foreach(messages, msg_idx, msg_max, message) {
            if (!yyjson_is_obj(message)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "eslint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path_str;
            event.execution_time = 0.0;
            
            // Extract line and column
            yyjson_val *line = yyjson_obj_get(message, "line");
            if (line && yyjson_is_num(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            yyjson_val *column = yyjson_obj_get(message, "column");
            if (column && yyjson_is_num(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Extract message
            yyjson_val *msg_text = yyjson_obj_get(message, "message");
            if (msg_text && yyjson_is_str(msg_text)) {
                event.message = yyjson_get_str(msg_text);
            }
            
            // Extract rule ID
            yyjson_val *rule_id = yyjson_obj_get(message, "ruleId");
            if (rule_id && yyjson_is_str(rule_id)) {
                event.error_code = yyjson_get_str(rule_id);
                event.function_name = yyjson_get_str(rule_id);
            }
            
            // Map severity to status
            yyjson_val *severity = yyjson_obj_get(message, "severity");
            if (severity && yyjson_is_num(severity)) {
                int sev = yyjson_get_int(severity);
                switch (sev) {
                    case 2:
                        event.status = ValidationEventStatus::ERROR;
                        event.category = "lint_error";
                        event.severity = "error";
                        break;
                    case 1:
                        event.status = ValidationEventStatus::WARNING;
                        event.category = "lint_warning"; 
                        event.severity = "warning";
                        break;
                    default:
                        event.status = ValidationEventStatus::INFO;
                        event.category = "lint_info";
                        event.severity = "info";
                        break;
                }
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.category = "lint_warning";
                event.severity = "warning";
            }
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
    return events;
}


} // namespace duckdb