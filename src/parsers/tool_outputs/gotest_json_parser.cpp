#include "gotest_json_parser.hpp"
#include <sstream>

namespace duckdb {

using namespace duckdb_yyjson;

bool GoTestJSONParser::canParse(const std::string& content) const {
    // Look for Go test JSON patterns (one JSON per line)
    if (content.find("\"Action\"") == std::string::npos ||
        content.find("\"Package\"") == std::string::npos) {
        return false;
    }
    
    return isValidGoTestJSON(content);
}

bool GoTestJSONParser::isValidGoTestJSON(const std::string& content) const {
    std::istringstream stream(content);
    std::string line;
    
    // Check first non-empty line
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) {
            return false;
        }
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        bool is_valid = false;
        
        if (yyjson_is_obj(root)) {
            yyjson_val *action = yyjson_obj_get(root, "Action");
            yyjson_val *package = yyjson_obj_get(root, "Package");
            
            is_valid = (action && yyjson_is_str(action)) &&
                      (package && yyjson_is_str(package));
        }
        
        yyjson_doc_free(doc);
        return is_valid;
    }
    
    return false;
}

std::vector<ValidationEvent> GoTestJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Track test results
    std::map<std::string, ValidationEvent> test_events;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        // Parse each JSON line
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) continue;
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        // Extract fields
        yyjson_val *action = yyjson_obj_get(root, "Action");
        yyjson_val *package = yyjson_obj_get(root, "Package");
        yyjson_val *test = yyjson_obj_get(root, "Test");
        yyjson_val *elapsed = yyjson_obj_get(root, "Elapsed");
        
        if (!action || !yyjson_is_str(action)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        std::string action_str = yyjson_get_str(action);
        std::string package_str = package && yyjson_is_str(package) ? yyjson_get_str(package) : "";
        std::string test_str = test && yyjson_is_str(test) ? yyjson_get_str(test) : "";
        
        // Create unique test key
        std::string test_key = package_str + "::" + test_str;
        
        if (action_str == "run" && !test_str.empty()) {
            // Initialize test event
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "go_test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.file_path = package_str;
            event.test_name = test_str;
            event.function_name = test_str;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            
            test_events[test_key] = event;
        } else if ((action_str == "pass" || action_str == "fail" || action_str == "skip") && !test_str.empty()) {
            // Finalize test event
            if (test_events.find(test_key) != test_events.end()) {
                ValidationEvent &event = test_events[test_key];
                
                if (elapsed && yyjson_is_num(elapsed)) {
                    event.execution_time = yyjson_get_real(elapsed);
                }
                
                if (action_str == "pass") {
                    event.status = ValidationEventStatus::PASS;
                    event.category = "test_success";
                    event.severity = "info";
                    event.message = "Test passed";
                } else if (action_str == "fail") {
                    event.status = ValidationEventStatus::FAIL;
                    event.category = "test_failure";
                    event.severity = "error";
                    event.message = "Test failed";
                } else if (action_str == "skip") {
                    event.status = ValidationEventStatus::SKIP;
                    event.category = "test_skipped";
                    event.severity = "warning";
                    event.message = "Test skipped";
                }
                
                events.push_back(event);
                test_events.erase(test_key);
            }
        }
        
        yyjson_doc_free(doc);
    }
    
    return events;
}


} // namespace duckdb