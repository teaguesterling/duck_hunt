#include "rubocop_json_parser.hpp"
#include "../../core/parser_registry.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool RuboCopJSONParser::canParse(const std::string& content) const {
    // Quick checks for RuboCop JSON patterns
    if (content.find("\"files\"") != std::string::npos &&
        content.find("\"offenses\"") != std::string::npos &&
        content.find("\"cop_name\"") != std::string::npos) {
        return isValidRuboCopJSON(content);
    }
    return false;
}

bool RuboCopJSONParser::isValidRuboCopJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_obj(root)) {
        yyjson_val *files = yyjson_obj_get(root, "files");
        if (files && yyjson_is_arr(files)) {
            is_valid = true;
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> RuboCopJSONParser::parse(const std::string& content) const {
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
    
    // Get files array
    yyjson_val *files = yyjson_obj_get(root, "files");
    if (!files || !yyjson_is_arr(files)) {
        yyjson_doc_free(doc);
        return events;
    }
    
    // Parse each file
    size_t idx, max;
    yyjson_val *file;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(files, idx, max, file) {
        if (!yyjson_is_obj(file)) continue;
        
        // Get file path
        yyjson_val *path = yyjson_obj_get(file, "path");
        if (!path || !yyjson_is_str(path)) continue;
        
        std::string file_path = yyjson_get_str(path);
        
        // Get offenses array
        yyjson_val *offenses = yyjson_obj_get(file, "offenses");
        if (!offenses || !yyjson_is_arr(offenses)) continue;
        
        // Parse each offense
        size_t offense_idx, offense_max;
        yyjson_val *offense;
        
        yyjson_arr_foreach(offenses, offense_idx, offense_max, offense) {
            if (!yyjson_is_obj(offense)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "rubocop";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.category = "code_quality";
            
            // Get severity
            yyjson_val *severity = yyjson_obj_get(offense, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                } else if (severity_str == "convention") {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "convention";
                } else {
                    event.status = ValidationEventStatus::INFO;
                    event.severity = severity_str;
                }
            }
            
            // Get message
            yyjson_val *message = yyjson_obj_get(offense, "message");
            if (message && yyjson_is_str(message)) {
                event.message = yyjson_get_str(message);
            }
            
            // Get cop name (rule ID)
            yyjson_val *cop_name = yyjson_obj_get(offense, "cop_name");
            if (cop_name && yyjson_is_str(cop_name)) {
                event.error_code = yyjson_get_str(cop_name);
            }
            
            // Get location
            yyjson_val *location = yyjson_obj_get(offense, "location");
            if (location && yyjson_is_obj(location)) {
                yyjson_val *start_line = yyjson_obj_get(location, "start_line");
                yyjson_val *start_column = yyjson_obj_get(location, "start_column");
                
                if (start_line && yyjson_is_num(start_line)) {
                    event.line_number = yyjson_get_int(start_line);
                }
                if (start_column && yyjson_is_num(start_column)) {
                    event.column_number = yyjson_get_int(start_column);
                }
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "{\"tool\": \"rubocop\", \"cop_name\": \"" + event.error_code + "\"}";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
    return events;
}

// Auto-register this parser
REGISTER_PARSER(RuboCopJSONParser);

} // namespace duckdb