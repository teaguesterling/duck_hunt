#include "stylelint_json_parser.hpp"
#include "core/legacy_parser_registry.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool StylelintJSONParser::canParse(const std::string& content) const {
    // Quick checks for Stylelint JSON patterns
    if (content.find("\"source\"") != std::string::npos &&
        content.find("\"warnings\"") != std::string::npos &&
        content.find("\"rule\"") != std::string::npos &&
        content.find("\"text\"") != std::string::npos) {
        return isValidStylelintJSON(content);
    }
    return false;
}

bool StylelintJSONParser::isValidStylelintJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_arr(root)) {
        // Check if first element has Stylelint structure
        size_t idx, max;
        yyjson_val *file_result;
        yyjson_arr_foreach(root, idx, max, file_result) {
            if (yyjson_is_obj(file_result)) {
                yyjson_val *source = yyjson_obj_get(file_result, "source");
                yyjson_val *warnings = yyjson_obj_get(file_result, "warnings");
                
                if (source && yyjson_is_str(source) &&
                    warnings && yyjson_is_arr(warnings)) {
                    is_valid = true;
                    break;
                }
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> StylelintJSONParser::parse(const std::string& content) const {
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
    
    // Parse each file result
    size_t idx, max;
    yyjson_val *file_result;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(root, idx, max, file_result) {
        if (!yyjson_is_obj(file_result)) continue;
        
        // Get source file path
        yyjson_val *source = yyjson_obj_get(file_result, "source");
        if (!source || !yyjson_is_str(source)) continue;
        
        std::string file_path = yyjson_get_str(source);
        
        // Get warnings array
        yyjson_val *warnings = yyjson_obj_get(file_result, "warnings");
        if (!warnings || !yyjson_is_arr(warnings)) continue;
        
        // Parse each warning
        size_t warn_idx, warn_max;
        yyjson_val *warning;
        
        yyjson_arr_foreach(warnings, warn_idx, warn_max, warning) {
            if (!yyjson_is_obj(warning)) continue;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "stylelint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "css_style";
            event.file_path = file_path;
            event.execution_time = 0.0;
            
            // Get line number
            yyjson_val *line = yyjson_obj_get(warning, "line");
            if (line && yyjson_is_num(line)) {
                event.line_number = yyjson_get_int(line);
            } else {
                event.line_number = -1;
            }
            
            // Get column number
            yyjson_val *column = yyjson_obj_get(warning, "column");
            if (column && yyjson_is_num(column)) {
                event.column_number = yyjson_get_int(column);
            } else {
                event.column_number = -1;
            }
            
            // Get severity
            yyjson_val *severity = yyjson_obj_get(warning, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                event.severity = severity_str;
                
                // Map stylelint severity to ValidationEventStatus
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                } else {
                    event.status = ValidationEventStatus::WARNING;
                }
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            // Get rule name
            yyjson_val *rule = yyjson_obj_get(warning, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.error_code = yyjson_get_str(rule);
            }
            
            // Get message text
            yyjson_val *text = yyjson_obj_get(warning, "text");
            if (text && yyjson_is_str(text)) {
                event.message = yyjson_get_str(text);
            }
            
            // Get end line/column for range information
            yyjson_val *end_line = yyjson_obj_get(warning, "endLine");
            yyjson_val *end_column = yyjson_obj_get(warning, "endColumn");
            if (end_line && yyjson_is_num(end_line) && end_column && yyjson_is_num(end_column)) {
                event.suggestion = "Range: " + std::to_string(yyjson_get_int(end_line)) + ":" + std::to_string(yyjson_get_int(end_column));
            }
            
            // Set raw output and structured data
            event.raw_output = content;
            event.structured_data = "{\"tool\": \"stylelint\", \"rule\": \"" + event.error_code + "\", \"severity\": \"" + event.severity + "\"}";
            
            events.push_back(event);
        }
    }
    
    yyjson_doc_free(doc);
    return events;
}

// Auto-register this parser
REGISTER_PARSER(StylelintJSONParser);

} // namespace duckdb