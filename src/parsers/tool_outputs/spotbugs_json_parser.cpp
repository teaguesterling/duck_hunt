#include "spotbugs_json_parser.hpp"
#include "core/legacy_parser_registry.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool SpotBugsJSONParser::canParse(const std::string& content) const {
    // Quick checks for SpotBugs JSON patterns
    if (content.find("\"BugCollection\"") != std::string::npos &&
        content.find("\"BugInstance\"") != std::string::npos &&
        content.find("\"type\"") != std::string::npos &&
        content.find("\"priority\"") != std::string::npos) {
        return isValidSpotBugsJSON(content);
    }
    return false;
}

bool SpotBugsJSONParser::isValidSpotBugsJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;
    
    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;
    
    if (yyjson_is_obj(root)) {
        yyjson_val *bug_collection = yyjson_obj_get(root, "BugCollection");
        if (bug_collection && yyjson_is_obj(bug_collection)) {
            yyjson_val *bug_instances = yyjson_obj_get(bug_collection, "BugInstance");
            if (bug_instances && yyjson_is_arr(bug_instances)) {
                is_valid = true;
            }
        }
    }
    
    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> SpotBugsJSONParser::parse(const std::string& content) const {
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
    
    // Get BugCollection object
    yyjson_val *bug_collection = yyjson_obj_get(root, "BugCollection");
    if (!bug_collection || !yyjson_is_obj(bug_collection)) {
        yyjson_doc_free(doc);
        return events; // No bug collection to process
    }
    
    // Get BugInstance array
    yyjson_val *bug_instances = yyjson_obj_get(bug_collection, "BugInstance");
    if (!bug_instances || !yyjson_is_arr(bug_instances)) {
        yyjson_doc_free(doc);
        return events; // No bug instances to process
    }
    
    // Parse each bug instance
    size_t idx, max;
    yyjson_val *bug;
    int64_t event_id = 1;
    
    yyjson_arr_foreach(bug_instances, idx, max, bug) {
        if (!yyjson_is_obj(bug)) continue;
        
        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "spotbugs";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.execution_time = 0.0;
        
        // Get bug type as error code
        yyjson_val *type = yyjson_obj_get(bug, "type");
        if (type && yyjson_is_str(type)) {
            event.error_code = yyjson_get_str(type);
        }
        
        // Get category and map event type
        yyjson_val *category = yyjson_obj_get(bug, "category");
        if (category && yyjson_is_str(category)) {
            std::string category_str = yyjson_get_str(category);
            
            // Map SpotBugs categories to event types
            if (category_str == "SECURITY") {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (category_str == "PERFORMANCE") {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            } else if (category_str == "CORRECTNESS") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "correctness";
            } else if (category_str == "BAD_PRACTICE") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "code_quality";
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.category = "static_analysis";
            }
        } else {
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "static_analysis";
        }
        
        // Get priority and map to status
        yyjson_val *priority = yyjson_obj_get(bug, "priority");
        if (priority && yyjson_is_str(priority)) {
            std::string priority_str = yyjson_get_str(priority);
            event.severity = priority_str;
            
            // Map SpotBugs priority to ValidationEventStatus (1=highest, 3=lowest)
            if (priority_str == "1") {
                event.status = ValidationEventStatus::ERROR;
            } else if (priority_str == "2") {
                event.status = ValidationEventStatus::WARNING;
            } else { // priority 3
                event.status = ValidationEventStatus::INFO;
            }
        } else {
            event.severity = "2";
            event.status = ValidationEventStatus::WARNING;
        }
        
        // Get short message as main message
        yyjson_val *short_message = yyjson_obj_get(bug, "ShortMessage");
        if (short_message && yyjson_is_str(short_message)) {
            event.message = yyjson_get_str(short_message);
        }
        
        // Get long message as suggestion
        yyjson_val *long_message = yyjson_obj_get(bug, "LongMessage");
        if (long_message && yyjson_is_str(long_message)) {
            event.suggestion = yyjson_get_str(long_message);
        }
        
        // Get source line information
        yyjson_val *source_line = yyjson_obj_get(bug, "SourceLine");
        if (source_line && yyjson_is_obj(source_line)) {
            // Check if this is the primary source line
            yyjson_val *primary = yyjson_obj_get(source_line, "primary");
            if (primary && yyjson_is_bool(primary) && yyjson_get_bool(primary)) {
                // Get source path
                yyjson_val *sourcepath = yyjson_obj_get(source_line, "sourcepath");
                if (sourcepath && yyjson_is_str(sourcepath)) {
                    event.file_path = yyjson_get_str(sourcepath);
                }
                
                // Get line number (use start line)
                yyjson_val *start_line = yyjson_obj_get(source_line, "start");
                if (start_line && yyjson_is_str(start_line)) {
                    try {
                        event.line_number = std::stoll(yyjson_get_str(start_line));
                    } catch (...) {
                        event.line_number = -1;
                    }
                } else {
                    event.line_number = -1;
                }
                
                // SpotBugs doesn't provide column information
                event.column_number = -1;
            }
        }
        
        // Get method information for function context
        yyjson_val *method = yyjson_obj_get(bug, "Method");
        if (method && yyjson_is_obj(method)) {
            yyjson_val *primary = yyjson_obj_get(method, "primary");
            if (primary && yyjson_is_bool(primary) && yyjson_get_bool(primary)) {
                yyjson_val *method_name = yyjson_obj_get(method, "name");
                yyjson_val *classname = yyjson_obj_get(method, "classname");
                
                if (method_name && yyjson_is_str(method_name) && classname && yyjson_is_str(classname)) {
                    event.function_name = std::string(yyjson_get_str(classname)) + "." + std::string(yyjson_get_str(method_name));
                }
            }
        }
        
        // Set raw output and structured data
        event.raw_output = content;
        event.structured_data = "{\"tool\": \"spotbugs\", \"type\": \"" + event.error_code + "\", \"priority\": \"" + event.severity + "\", \"category\": \"" + event.category + "\"}";
        
        events.push_back(event);
    }
    
    yyjson_doc_free(doc);
    return events;
}

// Auto-register this parser
REGISTER_PARSER(SpotBugsJSONParser);

} // namespace duckdb