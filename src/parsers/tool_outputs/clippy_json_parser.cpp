#include "clippy_json_parser.hpp"
#include "../../core/parser_registry.hpp"
#include "yyjson.hpp"
#include <sstream>

namespace duckdb {

using namespace duckdb_yyjson;

bool ClippyJSONParser::canParse(const std::string& content) const {
    // Quick checks for Clippy JSON patterns (JSONL format)
    if (content.find("\"message\"") != std::string::npos &&
        content.find("\"spans\"") != std::string::npos &&
        content.find("\"is_primary\"") != std::string::npos &&
        (content.find("\"level\":\"warn\"") != std::string::npos || 
         content.find("\"level\":\"error\"") != std::string::npos)) {
        return isValidClippyJSON(content);
    }
    return false;
}

bool ClippyJSONParser::isValidClippyJSON(const std::string& content) const {
    std::istringstream stream(content);
    std::string line;
    
    // Check first few lines for valid Clippy JSON format
    int line_count = 0;
    while (std::getline(stream, line) && line_count < 5) {
        if (line.empty()) continue;
        line_count++;
        
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) continue;
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (yyjson_is_obj(root)) {
            yyjson_val *message = yyjson_obj_get(root, "message");
            if (message && yyjson_is_obj(message)) {
                yyjson_val *spans = yyjson_obj_get(message, "spans");
                if (spans && yyjson_is_arr(spans)) {
                    yyjson_doc_free(doc);
                    return true;
                }
            }
        }
        yyjson_doc_free(doc);
    }
    
    return false;
}

std::vector<ValidationEvent> ClippyJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    
    // Parse JSONL format (JSON Lines) - each line is a separate JSON object
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Parse JSON using yyjson
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) {
            continue; // Skip invalid JSON lines
        }
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get message object
        yyjson_val *message = yyjson_obj_get(root, "message");
        if (!message || !yyjson_is_obj(message)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get spans array
        yyjson_val *spans = yyjson_obj_get(message, "spans");
        if (!spans || !yyjson_is_arr(spans)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Get primary span (first span with is_primary = true)
        yyjson_val *primary_span = nullptr;
        size_t idx, max;
        yyjson_val *span;
        
        yyjson_arr_foreach(spans, idx, max, span) {
            if (!yyjson_is_obj(span)) continue;
            
            yyjson_val *is_primary = yyjson_obj_get(span, "is_primary");
            if (is_primary && yyjson_is_bool(is_primary) && yyjson_get_bool(is_primary)) {
                primary_span = span;
                break;
            }
        }
        
        if (!primary_span) {
            // If no primary span found, use the first span
            primary_span = yyjson_arr_get_first(spans);
        }
        
        if (!primary_span) {
            yyjson_doc_free(doc);
            continue;
        }
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "clippy";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "code_quality";
        event.execution_time = 0.0;
        
        // Get file name from primary span
        yyjson_val *file_name = yyjson_obj_get(primary_span, "file_name");
        if (file_name && yyjson_is_str(file_name)) {
            event.file_path = yyjson_get_str(file_name);
        }
        
        // Get line number from primary span
        yyjson_val *line_start = yyjson_obj_get(primary_span, "line_start");
        if (line_start && yyjson_is_num(line_start)) {
            event.line_number = yyjson_get_int(line_start);
        } else {
            event.line_number = -1;
        }
        
        // Get column number from primary span
        yyjson_val *column_start = yyjson_obj_get(primary_span, "column_start");
        if (column_start && yyjson_is_num(column_start)) {
            event.column_number = yyjson_get_int(column_start);
        } else {
            event.column_number = -1;
        }
        
        // Get severity level
        yyjson_val *level = yyjson_obj_get(message, "level");
        if (level && yyjson_is_str(level)) {
            std::string level_str = yyjson_get_str(level);
            event.severity = level_str;
            
            // Map clippy levels to ValidationEventStatus
            if (level_str == "error") {
                event.status = ValidationEventStatus::ERROR;
            } else if (level_str == "warn" || level_str == "warning") {
                event.status = ValidationEventStatus::WARNING;
            } else if (level_str == "note" || level_str == "info") {
                event.status = ValidationEventStatus::INFO;
            } else {
                event.status = ValidationEventStatus::WARNING;
            }
        } else {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get code object for error code
        yyjson_val *code = yyjson_obj_get(message, "code");
        if (code && yyjson_is_obj(code)) {
            yyjson_val *code_str = yyjson_obj_get(code, "code");
            if (code_str && yyjson_is_str(code_str)) {
                event.error_code = yyjson_get_str(code_str);
            }
        }
        
        // Get message text
        yyjson_val *message_text = yyjson_obj_get(message, "message");
        if (message_text && yyjson_is_str(message_text)) {
            event.message = yyjson_get_str(message_text);
        }
        
        // Get suggestion from primary span
        yyjson_val *suggested_replacement = yyjson_obj_get(primary_span, "suggested_replacement");
        if (suggested_replacement && yyjson_is_str(suggested_replacement)) {
            event.suggestion = yyjson_get_str(suggested_replacement);
        }
        
        // Set raw output and structured data
        event.raw_output = line; // Use the specific line for this issue
        event.structured_data = "{\"tool\": \"clippy\", \"level\": \"" + event.severity + "\", \"code\": \"" + event.error_code + "\"}";
        
        events.push_back(event);
        
        yyjson_doc_free(doc);
    }
    
    return events;
}

// Auto-register this parser
REGISTER_PARSER(ClippyJSONParser);

} // namespace duckdb