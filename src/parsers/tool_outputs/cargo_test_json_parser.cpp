#include "cargo_test_json_parser.hpp"
#include "yyjson.hpp"
#include <sstream>
#include <map>

namespace duckdb {

using namespace duckdb_yyjson;

bool CargoTestJSONParser::canParse(const std::string& content) const {
    // Quick checks for Cargo test JSON patterns (JSONL format)
    if (content.find("\"type\":\"test\"") != std::string::npos &&
        content.find("\"event\":") != std::string::npos &&
        (content.find("\"started\"") != std::string::npos || 
         content.find("\"ok\"") != std::string::npos ||
         content.find("\"failed\"") != std::string::npos)) {
        return isValidCargoTestJSON(content);
    }
    return false;
}

bool CargoTestJSONParser::isValidCargoTestJSON(const std::string& content) const {
    std::istringstream stream(content);
    std::string line;
    
    // Check first few lines for valid Cargo test JSON format
    int line_count = 0;
    while (std::getline(stream, line) && line_count < 5) {
        if (line.empty()) continue;
        line_count++;
        
        yyjson_doc *doc = yyjson_read(line.c_str(), line.length(), 0);
        if (!doc) continue;
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (yyjson_is_obj(root)) {
            yyjson_val *type = yyjson_obj_get(root, "type");
            yyjson_val *event = yyjson_obj_get(root, "event");
            
            if (type && yyjson_is_str(type) && event && yyjson_is_str(event)) {
                std::string type_str = yyjson_get_str(type);
                if (type_str == "test" || type_str == "suite") {
                    yyjson_doc_free(doc);
                    return true;
                }
            }
        }
        yyjson_doc_free(doc);
    }
    
    return false;
}

std::vector<ValidationEvent> CargoTestJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Track test events
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
        
        // Get type and event
        yyjson_val *type = yyjson_obj_get(root, "type");
        yyjson_val *event_val = yyjson_obj_get(root, "event");
        
        if (!type || !yyjson_is_str(type) || !event_val || !yyjson_is_str(event_val)) {
            yyjson_doc_free(doc);
            continue;
        }
        
        std::string type_str = yyjson_get_str(type);
        std::string event_str = yyjson_get_str(event_val);
        
        // Handle test events
        if (type_str == "test") {
            yyjson_val *name = yyjson_obj_get(root, "name");
            if (!name || !yyjson_is_str(name)) {
                yyjson_doc_free(doc);
                continue;
            }
            
            std::string test_name = yyjson_get_str(name);
            
            if (event_str == "started") {
                // Initialize test event
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo_test";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = test_name;
                event.function_name = test_name;
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.category = "test";
                
                test_events[test_name] = event;
            } else if (event_str == "ok" || event_str == "failed" || event_str == "ignored") {
                // Finalize test event
                if (test_events.find(test_name) != test_events.end()) {
                    ValidationEvent &event = test_events[test_name];
                    
                    // Get execution time
                    yyjson_val *exec_time = yyjson_obj_get(root, "exec_time");
                    if (exec_time && yyjson_is_num(exec_time)) {
                        event.execution_time = yyjson_get_real(exec_time);
                    }
                    
                    // Set status based on event
                    if (event_str == "ok") {
                        event.status = ValidationEventStatus::PASS;
                        event.message = "Test passed";
                        event.severity = "success";
                    } else if (event_str == "failed") {
                        event.status = ValidationEventStatus::FAIL;
                        event.message = "Test failed";
                        event.severity = "error";
                        
                        // Get failure details from stdout
                        yyjson_val *stdout_val = yyjson_obj_get(root, "stdout");
                        if (stdout_val && yyjson_is_str(stdout_val)) {
                            std::string stdout_str = yyjson_get_str(stdout_val);
                            if (!stdout_str.empty()) {
                                event.message = "Test failed: " + stdout_str;
                            }
                        }
                    } else if (event_str == "ignored") {
                        event.status = ValidationEventStatus::SKIP;
                        event.message = "Test ignored";
                        event.severity = "info";
                    }
                    
                    // Set raw output and structured data
                    event.raw_output = line; // Use the specific line for this test
                    event.structured_data = "{\"tool\": \"cargo_test\", \"event\": \"" + event_str + "\"}";
                    
                    events.push_back(event);
                    test_events.erase(test_name);
                }
            }
        }
        
        yyjson_doc_free(doc);
    }
    
    return events;
}


} // namespace duckdb