#include "phpstan_json_parser.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool PHPStanJSONParser::canParse(const std::string& content) const {
    // Quick checks for PHPStan JSON patterns
    if (content.find("\"files\"") != std::string::npos &&
        content.find("\"messages\"") != std::string::npos &&
        content.find("\"ignorable\"") != std::string::npos) {
        return isValidPHPStanJSON(content);
    }
    return false;
}

bool PHPStanJSONParser::isValidPHPStanJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_obj(root)) {
        yyjson_val *files = yyjson_obj_get(root, "files");
        if (files && yyjson_is_obj(files)) {
            // Check if at least one file has messages structure
            size_t idx, max;
            yyjson_val *file_key, *file_data;
            yyjson_obj_foreach(files, idx, max, file_key, file_data) {
                if (yyjson_is_obj(file_data)) {
                    yyjson_val *messages = yyjson_obj_get(file_data, "messages");
                    if (messages && yyjson_is_arr(messages)) {
                        is_valid = true;
                        break;
                    }
                }
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> PHPStanJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        return events; // Return empty instead of throwing
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return events;
    }
    
    // Get files object
    yyjson_val *files = yyjson_obj_get(root, "files");
    if (!files || !yyjson_is_obj(files)) {
        yyjson_doc_free(doc);
        return events;
    }
    
    // Parse each file
    size_t idx, max;
    yyjson_val *file_key, *file_data;
    int64_t event_id = 1;
    
    yyjson_obj_foreach(files, idx, max, file_key, file_data) {
        if (!yyjson_is_str(file_key) || !yyjson_is_obj(file_data)) continue;
        
        std::string file_path = yyjson_get_str(file_key);
        
        // Get messages array
        yyjson_val *messages = yyjson_obj_get(file_data, "messages");
        if (!messages || !yyjson_is_arr(messages)) continue;
        
        // Parse each message
        size_t msg_idx, msg_max;
        yyjson_val *message;
        
        yyjson_arr_foreach(messages, msg_idx, msg_max, message) {
            if (!yyjson_is_obj(message)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "phpstan";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.category = "static_analysis";
            
            // Get message text
            yyjson_val *msg_text = yyjson_obj_get(message, "message");
            if (msg_text && yyjson_is_str(msg_text)) {
                event.message = yyjson_get_str(msg_text);
            }
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(message, "line");
            if (line && yyjson_is_num(line)) {
                event.line_number = yyjson_get_int(line);
            }
            
            // Get ignorable status (use as severity indicator)
            yyjson_val *ignorable = yyjson_obj_get(message, "ignorable");
            if (ignorable && yyjson_is_bool(ignorable)) {
                if (yyjson_get_bool(ignorable)) {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                } else {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                }
            } else {
                // Default to error
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            }
            
            // Get tip if available (use as suggestion)
            yyjson_val *tip = yyjson_obj_get(message, "tip");
            if (tip && yyjson_is_str(tip)) {
                event.suggestion = yyjson_get_str(tip);
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            std::string ignorable_str = (ignorable && yyjson_is_bool(ignorable) ? 
                                        (yyjson_get_bool(ignorable) ? "true" : "false") : "null");
            event.structured_data = "{\"tool\": \"phpstan\", \"ignorable\": " + ignorable_str + "}";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
    return events;
}


} // namespace duckdb