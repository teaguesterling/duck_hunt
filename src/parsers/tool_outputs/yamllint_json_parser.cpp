#include "yamllint_json_parser.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool YamllintJSONParser::canParse(const std::string& content) const {
    // Quick checks for Yamllint JSON patterns
    if (content.find("\"file\"") != std::string::npos &&
        content.find("\"line\"") != std::string::npos &&
        content.find("\"column\"") != std::string::npos &&
        content.find("\"rule\"") != std::string::npos &&
        content.find("\"level\"") != std::string::npos) {
        return isValidYamllintJSON(content);
    }
    return false;
}

bool YamllintJSONParser::isValidYamllintJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_arr(root)) {
        // Check if first element has Yamllint structure
        size_t idx, max;
        yyjson_val *issue;
        yyjson_arr_foreach(root, idx, max, issue) {
            if (yyjson_is_obj(issue)) {
                yyjson_val *file = yyjson_obj_get(issue, "file");
                yyjson_val *line = yyjson_obj_get(issue, "line");
                yyjson_val *rule = yyjson_obj_get(issue, "rule");
                yyjson_val *level = yyjson_obj_get(issue, "level");
                
                if (file && yyjson_is_str(file) &&
                    line && yyjson_is_num(line) &&
                    rule && yyjson_is_str(rule) &&
                    level && yyjson_is_str(level)) {
                    is_valid = true;
                    break;
                }
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> YamllintJSONParser::parse(const std::string& content) const {
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
    
    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "yamllint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "configuration";
        event.execution_time = 0.0;
        
        // Get file path
        yyjson_val *file = yyjson_obj_get(issue, "file");
        if (file && yyjson_is_str(file)) {
            event.file_path = yyjson_get_str(file);
        }
        
        // Get line number
        yyjson_val *line = yyjson_obj_get(issue, "line");
        if (line && yyjson_is_num(line)) {
            event.line_number = yyjson_get_int(line);
        } else {
            event.line_number = -1;
        }
        
        // Get column number
        yyjson_val *column = yyjson_obj_get(issue, "column");
        if (column && yyjson_is_num(column)) {
            event.column_number = yyjson_get_int(column);
        } else {
            event.column_number = -1;
        }
        
        // Get severity level
        yyjson_val *level = yyjson_obj_get(issue, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map yamllint levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get rule name as error code
        yyjson_val *rule = yyjson_obj_get(issue, "rule");
        if (rule && yyjson_is_str(rule)) {
            event.error_code = yyjson_get_str(rule);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get type as additional context
        yyjson_val *type = yyjson_obj_get(issue, "type");
        if (type && yyjson_is_str(type)) {
            event.suggestion = "Issue type: " + std::string(yyjson_get_str(type));
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "{\"tool\": \"yamllint\", \"rule\": \"" + event.error_code + "\", \"level\": \"" + event.severity + "\"}";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
    return events;
}


} // namespace duckdb