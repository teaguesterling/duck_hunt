#include "markdownlint_json_parser.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool MarkdownlintJSONParser::canParse(const std::string& content) const {
    // Quick checks for Markdownlint JSON patterns
    if (content.find("\"fileName\"") != std::string::npos &&
        content.find("\"lineNumber\"") != std::string::npos &&
        content.find("\"ruleNames\"") != std::string::npos &&
        content.find("\"ruleDescription\"") != std::string::npos) {
        return isValidMarkdownlintJSON(content);
    }
    return false;
}

bool MarkdownlintJSONParser::isValidMarkdownlintJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_arr(root)) {
        // Check if first element has Markdownlint structure
        size_t idx, max;
        yyjson_val *issue;
        yyjson_arr_foreach(root, idx, max, issue) {
            if (yyjson_is_obj(issue)) {
                yyjson_val *fileName = yyjson_obj_get(issue, "fileName");
                yyjson_val *lineNumber = yyjson_obj_get(issue, "lineNumber");
                yyjson_val *ruleNames = yyjson_obj_get(issue, "ruleNames");
                
                if (fileName && yyjson_is_str(fileName) &&
                    lineNumber && yyjson_is_num(lineNumber) &&
                    ruleNames && yyjson_is_arr(ruleNames)) {
                    is_valid = true;
                    break;
                }
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> MarkdownlintJSONParser::parse(const std::string& content) const {
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
        event.tool_name = "markdownlint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "documentation";
        event.execution_time = 0.0;
        
        // Get file name
        yyjson_val *file_name = yyjson_obj_get(issue, "fileName");
        if (file_name && yyjson_is_str(file_name)) {
            event.ref_file = yyjson_get_str(file_name);
        }
        
        // Get line number
        yyjson_val *line_number = yyjson_obj_get(issue, "lineNumber");
        if (line_number && yyjson_is_num(line_number)) {
            event.ref_line = yyjson_get_int(line_number);
        } else {
            event.ref_line = -1;
        }
        
        // Markdownlint doesn't provide column in the same way - use errorRange if available
        yyjson_val *error_range = yyjson_obj_get(issue, "errorRange");
        if (error_range && yyjson_is_arr(error_range)) {
            yyjson_val *first_elem = yyjson_arr_get_first(error_range);
            if (first_elem && yyjson_is_num(first_elem)) {
                event.ref_column = yyjson_get_int(first_elem);
            } else {
                event.ref_column = -1;
            }
        } else {
            event.ref_column = -1;
        }
        
        // Markdownlint issues are typically warnings (unless they're really severe)
        event.severity = "warning";
        event.status = ValidationEventStatus::WARNING;
        
        // Get rule names (first rule name as error code)
        yyjson_val *rule_names = yyjson_obj_get(issue, "ruleNames");
        if (rule_names && yyjson_is_arr(rule_names)) {
            yyjson_val *first_rule = yyjson_arr_get_first(rule_names);
            if (first_rule && yyjson_is_str(first_rule)) {
                event.error_code = yyjson_get_str(first_rule);
            }
        }
        
        // Get rule description as message
        yyjson_val *rule_description = yyjson_obj_get(issue, "ruleDescription");
        if (rule_description && yyjson_is_str(rule_description)) {
            event.message = yyjson_get_str(rule_description);
        }
        
        // Get error detail as suggestion if available
        yyjson_val *error_detail = yyjson_obj_get(issue, "errorDetail");
        if (error_detail && yyjson_is_str(error_detail)) {
            event.suggestion = yyjson_get_str(error_detail);
        }
        
        // Set raw output and structured data
        event.log_content = content;
        event.structured_data = "{\"tool\": \"markdownlint\", \"rule\": \"" + event.error_code + "\"}";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
    return events;
}


} // namespace duckdb