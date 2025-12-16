#include "pytest_json_parser.hpp"
#include "include/validation_event_types.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool PytestJSONParser::canParse(const std::string& content) const {
    // Look for pytest JSON structure
    if (content.find("\"tests\"") == std::string::npos ||
        content.find("\"nodeid\"") == std::string::npos) {
        return false;
    }
    
    return isValidPytestJSON(content);
}

bool PytestJSONParser::isValidPytestJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        return false;
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_obj(root)) {
        yyjson_val *tests = yyjson_obj_get(root, "tests");
        if (tests && yyjson_is_arr(tests)) {
            // Check if first test has pytest structure
            size_t arr_size = yyjson_arr_size(tests);
            if (arr_size > 0) {
                yyjson_val *first_test = yyjson_arr_get_first(tests);
                if (yyjson_is_obj(first_test)) {
                    yyjson_val *nodeid = yyjson_obj_get(first_test, "nodeid");
                    yyjson_val *outcome = yyjson_obj_get(first_test, "outcome");
                    
                    is_valid = (nodeid && yyjson_is_str(nodeid)) &&
                              (outcome && yyjson_is_str(outcome));
                }
            } else {
                // Empty tests array is valid pytest output
                is_valid = true;
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> PytestJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    
    // Parse JSON using yyjson
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse pytest JSON");
    }
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid pytest JSON: root is not an object");
    }
    
    // Get tests array
    yyjson_val *tests = yyjson_obj_get(root, "tests");
    if (!tests || !yyjson_is_arr(tests)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid pytest JSON: no tests array found");
    }
    
    // Parse each test
    size_t idx, max;
    yyjson_val *test;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(tests, idx, max, test) {
        if (!yyjson_is_obj(test)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "pytest";
        event.event_type = ValidationEventType::TEST_RESULT;
        event.line_number = -1;
        event.column_number = -1;
        event.execution_time = 0.0;
        
        // Extract nodeid (test name with file path)
        yyjson_val *nodeid = yyjson_obj_get(test, "nodeid");
        if (nodeid && yyjson_is_str(nodeid)) {
            std::string nodeid_str = yyjson_get_str(nodeid);
            
            // Parse nodeid format: "file.py::test_function"
            size_t separator = nodeid_str.find("::");
            if (separator != std::string::npos) {
                event.file_path = nodeid_str.substr(0, separator);
                event.test_name = nodeid_str.substr(separator + 2);
                event.function_name = event.test_name;
            } else {
                event.test_name = nodeid_str;
                event.function_name = nodeid_str;
            }
        }
        
        // Extract outcome
        yyjson_val *outcome = yyjson_obj_get(test, "outcome");
        if (outcome && yyjson_is_str(outcome)) {
            std::string outcome_str = yyjson_get_str(outcome);
            event.status = StringToValidationEventStatus(outcome_str);
        } else {
            event.status = ValidationEventStatus::ERROR;
        }
        
        // Extract duration - check top level first, then inside call object
        yyjson_val *duration = yyjson_obj_get(test, "duration");
        if (duration && yyjson_is_num(duration)) {
            event.execution_time = yyjson_get_real(duration);
        }

        // Extract longrepr (error message) - check top level first
        yyjson_val *longrepr = yyjson_obj_get(test, "longrepr");
        if (longrepr && yyjson_is_str(longrepr)) {
            event.message = yyjson_get_str(longrepr);
        }

        // Also check inside call object (pytest-json-report format)
        yyjson_val *call = yyjson_obj_get(test, "call");
        if (call && yyjson_is_obj(call)) {
            // Duration from call (if not already set)
            if (event.execution_time == 0.0) {
                yyjson_val *call_duration = yyjson_obj_get(call, "duration");
                if (call_duration && yyjson_is_num(call_duration)) {
                    event.execution_time = yyjson_get_real(call_duration);
                }
            }

            // Longrepr from call (if not already set)
            if (event.message.empty()) {
                yyjson_val *call_longrepr = yyjson_obj_get(call, "longrepr");
                if (call_longrepr && yyjson_is_str(call_longrepr)) {
                    event.message = yyjson_get_str(call_longrepr);
                }
            }
        }
        
        // Set category and severity based on status
        switch (event.status) {
            case ValidationEventStatus::PASS:
                event.category = "test_success";
                event.severity = "info";
                if (event.message.empty()) event.message = "Test passed";
                break;
            case ValidationEventStatus::FAIL:
                event.category = "test_failure";
                event.severity = "error";
                if (event.message.empty()) event.message = "Test failed";
                break;
            case ValidationEventStatus::SKIP:
                event.category = "test_skipped";
                event.severity = "warning";
                if (event.message.empty()) event.message = "Test skipped";
                break;
            default:
                event.category = "test_error";
                event.severity = "error";
                if (event.message.empty()) event.message = "Test error";
                break;
        }
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
    return events;
}


} // namespace duckdb