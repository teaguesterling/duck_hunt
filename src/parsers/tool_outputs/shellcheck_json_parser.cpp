#include "shellcheck_json_parser.hpp"
#include "../../core/parser_registry.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool ShellCheckJSONParser::canParse(const std::string& content) const {
    // Quick checks for ShellCheck JSON patterns
    if (content.find("\"file\"") != std::string::npos &&
        content.find("\"level\"") != std::string::npos &&
        content.find("\"code\"") != std::string::npos &&
        content.find("\"message\"") != std::string::npos) {
        return isValidShellCheckJSON(content);
    }
    return false;
}

bool ShellCheckJSONParser::isValidShellCheckJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_arr(root)) {
        // Check if first element has ShellCheck structure
        size_t idx, max;
        yyjson_val *issue;
        yyjson_arr_foreach(root, idx, max, issue) {
            if (yyjson_is_obj(issue)) {
                yyjson_val *file = yyjson_obj_get(issue, "file");
                yyjson_val *level = yyjson_obj_get(issue, "level");
                yyjson_val *code = yyjson_obj_get(issue, "code");
                
                if (file && yyjson_is_str(file) &&
                    level && yyjson_is_str(level) &&
                    code && yyjson_is_str(code)) {
                    is_valid = true;
                    break;
                }
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> ShellCheckJSONParser::parse(const std::string& content) const {
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
        event.tool_name = "shellcheck";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "shell_script";
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
        
        // Get severity/level
        yyjson_val *level = yyjson_obj_get(issue, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map ShellCheck levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (level_str == "info") {
                event.status = ValidationEventStatus::INFO;
            } else if (level_str == "style") {
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get error code (SC#### codes)
        yyjson_val *code = yyjson_obj_get(issue, "code");
        if (code && yyjson_is_str(code)) {
            event.error_code = yyjson_get_str(code);
        }
        
        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }
        
        // Get fix suggestions if available
        yyjson_val *fix = yyjson_obj_get(issue, "fix");
        if (fix && yyjson_is_obj(fix)) {
            yyjson_val *replacements = yyjson_obj_get(fix, "replacements");
            if (replacements && yyjson_is_arr(replacements)) {
                event.suggestion = "Fix available";
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "{\"tool\": \"shellcheck\", \"code\": \"" + event.error_code + "\", \"level\": \"" + event.severity + "\"}";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
    return events;
}

// Auto-register this parser
REGISTER_PARSER(ShellCheckJSONParser);

} // namespace duckdb