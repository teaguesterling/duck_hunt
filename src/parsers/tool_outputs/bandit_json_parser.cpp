#include "bandit_json_parser.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool BanditJSONParser::canParse(const std::string& content) const {
    // Quick checks for Bandit JSON patterns
    if (content.find("\"results\"") != std::string::npos &&
        content.find("\"test_id\"") != std::string::npos &&
        content.find("\"issue_severity\"") != std::string::npos &&
        content.find("\"issue_text\"") != std::string::npos) {
        return isValidBanditJSON(content);
    }
    return false;
}

bool BanditJSONParser::isValidBanditJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_obj(root)) {
        yyjson_val *results = yyjson_obj_get(root, "results");
        if (results && yyjson_is_arr(results)) {
            // Check if first element has Bandit structure
            size_t idx, max;
            yyjson_val *issue;
            yyjson_arr_foreach(results, idx, max, issue) {
                if (yyjson_is_obj(issue)) {
                    yyjson_val *test_id = yyjson_obj_get(issue, "test_id");
                    yyjson_val *issue_severity = yyjson_obj_get(issue, "issue_severity");
                    
                    if (test_id && yyjson_is_str(test_id) &&
                        issue_severity && yyjson_is_str(issue_severity)) {
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

std::vector<ValidationEvent> BanditJSONParser::parse(const std::string& content) const {
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
    
    // Get results array
    yyjson_val *results = yyjson_obj_get(root, "results");
    if (!results || !yyjson_is_arr(results)) {
        yyjson_doc_free(doc);
        return events; // No results to process
    }
    
    // Parse each security issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(results, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "bandit";
        event.event_type = ValidationEventType::SECURITY_FINDING;
        event.category = "security";
        event.execution_time = 0.0;
        
        // Get file path
        yyjson_val *filename = yyjson_obj_get(issue, "filename");
        if (filename && yyjson_is_str(filename)) {
            event.file_path = yyjson_get_str(filename);
        }
        
        // Get line number
        yyjson_val *line_number = yyjson_obj_get(issue, "line_number");
        if (line_number && yyjson_is_num(line_number)) {
            event.line_number = yyjson_get_int(line_number);
        } else {
            event.line_number = -1;
        }
        
        // Get column offset (Bandit uses col_offset)
        yyjson_val *col_offset = yyjson_obj_get(issue, "col_offset");
        if (col_offset && yyjson_is_num(col_offset)) {
            event.column_number = yyjson_get_int(col_offset);
        } else {
            event.column_number = -1;
        }
        
        // Get test ID as error code
        yyjson_val *test_id = yyjson_obj_get(issue, "test_id");
        if (test_id && yyjson_is_str(test_id)) {
            event.error_code = yyjson_get_str(test_id);
        }
        
        // Get issue severity and map to status
        yyjson_val *issue_severity = yyjson_obj_get(issue, "issue_severity");
        if (issue_severity && yyjson_is_str(issue_severity)) {
            std::string severity_str = yyjson_get_str(issue_severity);
            event.severity = severity_str;
            
            // Map Bandit severity to ValidationEventStatus
            if (severity_str == "HIGH") {
                event.status = ValidationEventStatus::ERROR;
            } else if (severity_str == "MEDIUM") {
                event.status = ValidationEventStatus::WARNING;
            } else { // LOW
                event.status = ValidationEventStatus::INFO;
            }
        } else {
            event.severity = "MEDIUM";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get issue text as message
        yyjson_val *issue_text = yyjson_obj_get(issue, "issue_text");
        if (issue_text && yyjson_is_str(issue_text)) {
            event.message = yyjson_get_str(issue_text);
        }
        
        // Get test name as function context
        yyjson_val *test_name = yyjson_obj_get(issue, "test_name");
        if (test_name && yyjson_is_str(test_name)) {
            event.function_name = yyjson_get_str(test_name);
        }
        
        // Get CWE information for suggestion
        yyjson_val *issue_cwe = yyjson_obj_get(issue, "issue_cwe");
        if (issue_cwe && yyjson_is_obj(issue_cwe)) {
            yyjson_val *cwe_id = yyjson_obj_get(issue_cwe, "id");
            yyjson_val *cwe_link = yyjson_obj_get(issue_cwe, "link");
            
            if (cwe_id && yyjson_is_num(cwe_id)) {
                std::string suggestion = "CWE-" + std::to_string(yyjson_get_int(cwe_id));
                if (cwe_link && yyjson_is_str(cwe_link)) {
                    suggestion += ": " + std::string(yyjson_get_str(cwe_link));
                }
                event.suggestion = suggestion;
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "{\"tool\": \"bandit\", \"test_id\": \"" + event.error_code + "\", \"severity\": \"" + event.severity + "\"}";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
    return events;
}


} // namespace duckdb