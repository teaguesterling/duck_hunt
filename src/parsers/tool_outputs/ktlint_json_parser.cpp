#include "ktlint_json_parser.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool KtlintJSONParser::canParse(const std::string& content) const {
    // Quick checks for Ktlint JSON patterns
    if (content.find("\"file\"") != std::string::npos &&
        content.find("\"errors\"") != std::string::npos &&
        content.find("\"rule\"") != std::string::npos &&
        content.find("\"line\"") != std::string::npos &&
        content.find("\"column\"") != std::string::npos) {
        return isValidKtlintJSON(content);
    }
    return false;
}

bool KtlintJSONParser::isValidKtlintJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_arr(root)) {
        // Check if first element has Ktlint structure
        size_t idx, max;
        yyjson_val *file_entry;
        yyjson_arr_foreach(root, idx, max, file_entry) {
            if (yyjson_is_obj(file_entry)) {
                yyjson_val *file = yyjson_obj_get(file_entry, "file");
                yyjson_val *errors = yyjson_obj_get(file_entry, "errors");
                
                if (file && yyjson_is_str(file) &&
                    errors && yyjson_is_arr(errors)) {
                    is_valid = true;
                    break;
                }
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> KtlintJSONParser::parse(const std::string& content) const {
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
    
    // Parse each file entry
    size_t file_idx, file_max;
    yyjson_val *file_entry;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, file_idx, file_max, file_entry) {
        if (!yyjson_is_obj(file_entry)) continue;
        
        // Get file path
        std::string file_path;
        yyjson_val *file = yyjson_obj_get(file_entry, "file");
        if (file && yyjson_is_str(file)) {
            file_path = yyjson_get_str(file);
        }
        
        // Get errors array
        yyjson_val *errors = yyjson_obj_get(file_entry, "errors");
        if (!errors || !yyjson_is_arr(errors)) continue;
        
        // Parse each error
        size_t error_idx, error_max;
        yyjson_val *error;
        
        yyjson_arr_foreach(errors, error_idx, error_max, error) {
            if (!yyjson_is_obj(error)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "ktlint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "code_style";
            event.file_path = file_path;
            event.execution_time = 0.0;
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(error, "line");
            if (line && yyjson_is_num(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            // Get column number
            yyjson_val *column = yyjson_obj_get(error, "column");
            if (column && yyjson_is_num(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Get rule name as error code
            yyjson_val *rule = yyjson_obj_get(error, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.error_code = yyjson_get_str(rule);
            }
            
            // Get message
            yyjson_val *message = yyjson_obj_get(error, "message");
            if (message && yyjson_is_str(message)) {
                event.message = yyjson_get_str(message);
            }
            
            // ktlint doesn't provide explicit severity, so we infer from rule types
            std::string rule_str = event.error_code;
            if (rule_str.find("max-line-length") != std::string::npos || 
                rule_str.find("no-wildcard-imports") != std::string::npos) {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            } else if (rule_str.find("indent") != std::string::npos || 
                       rule_str.find("final-newline") != std::string::npos) {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else {
                // Default to warning for style issues
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            // Get detail information if available
            yyjson_val *detail = yyjson_obj_get(error, "detail");
            if (detail && yyjson_is_str(detail)) {
                event.suggestion = yyjson_get_str(detail);
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "{\"tool\": \"ktlint\", \"rule\": \"" + event.error_code + "\", \"severity\": \"" + event.severity + "\"}";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
    return events;
}


} // namespace duckdb