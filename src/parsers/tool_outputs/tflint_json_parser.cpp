#include "tflint_json_parser.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool TflintJSONParser::canParse(const std::string& content) const {
    // Quick heuristic checks before parsing
    if (content.empty() || content[0] != '{') {
        return false;
    }

    // Look for tflint-specific JSON structure indicators
    if (content.find("\"issues\"") == std::string::npos) {
        return false;
    }

    // Check for tflint-specific fields
    if (content.find("\"rule\"") == std::string::npos &&
        content.find("\"range\"") == std::string::npos) {
        return false;
    }

    return isValidTflintJSON(content);
}

bool TflintJSONParser::isValidTflintJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;

    if (yyjson_is_obj(root)) {
        // Check for tflint structure: issues array
        yyjson_val *issues = yyjson_obj_get(root, "issues");
        if (issues && yyjson_is_arr(issues)) {
            size_t arr_size = yyjson_arr_size(issues);
            if (arr_size > 0) {
                yyjson_val *first_issue = yyjson_arr_get_first(issues);
                if (yyjson_is_obj(first_issue)) {
                    // Check for issue structure: rule, message
                    yyjson_val *rule = yyjson_obj_get(first_issue, "rule");
                    yyjson_val *message = yyjson_obj_get(first_issue, "message");

                    is_valid = (rule && yyjson_is_obj(rule)) &&
                              (message && yyjson_is_str(message));
                }
            } else {
                // Empty issues array is valid (no issues found)
                is_valid = true;
            }
        }
    }

    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> TflintJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;

    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse tflint JSON");
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid tflint JSON: root is not an object");
    }

    // Get issues array
    yyjson_val *issues = yyjson_obj_get(root, "issues");
    if (!issues || !yyjson_is_arr(issues)) {
        yyjson_doc_free(doc);
        return events; // No issues to process
    }

    // Parse each issue
    size_t idx, max;
    yyjson_val *issue;
    int64_t event_id = 1;

    yyjson_arr_foreach(issues, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;

        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "tflint";
        event.event_type = ValidationEventType::LINT_ISSUE;
        event.category = "infrastructure";

        // Get rule information
        yyjson_val *rule = yyjson_obj_get(issue, "rule");
        if (rule && yyjson_is_obj(rule)) {
            // Get rule name as error code
            yyjson_val *rule_name = yyjson_obj_get(rule, "name");
            if (rule_name && yyjson_is_str(rule_name)) {
                event.error_code = yyjson_get_str(rule_name);
                event.function_name = yyjson_get_str(rule_name);
            }

            // Get severity
            yyjson_val *severity = yyjson_obj_get(rule, "severity");
            if (severity && yyjson_is_str(severity)) {
                std::string severity_str = yyjson_get_str(severity);
                event.severity = severity_str;

                // Map tflint severities to ValidationEventStatus
                if (severity_str == "error") {
                    event.status = ValidationEventStatus::ERROR;
                } else if (severity_str == "warning") {
                    event.status = ValidationEventStatus::WARNING;
                } else if (severity_str == "notice") {
                    event.status = ValidationEventStatus::INFO;
                } else {
                    event.status = ValidationEventStatus::WARNING;
                }
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
        }

        // Get message
        yyjson_val *message = yyjson_obj_get(issue, "message");
        if (message && yyjson_is_str(message)) {
            event.message = yyjson_get_str(message);
        }

        // Get range information
        yyjson_val *range = yyjson_obj_get(issue, "range");
        if (range && yyjson_is_obj(range)) {
            // Get filename
            yyjson_val *filename = yyjson_obj_get(range, "filename");
            if (filename && yyjson_is_str(filename)) {
                event.file_path = yyjson_get_str(filename);
            }

            // Get start position
            yyjson_val *start = yyjson_obj_get(range, "start");
            if (start && yyjson_is_obj(start)) {
                yyjson_val *line = yyjson_obj_get(start, "line");
                if (line && yyjson_is_int(line)) {
                    event.line_number = yyjson_get_int(line);
                } else {
                    event.line_number = -1;
                }

                yyjson_val *column = yyjson_obj_get(start, "column");
                if (column && yyjson_is_int(column)) {
                    event.column_number = yyjson_get_int(column);
                } else {
                    event.column_number = -1;
                }
            }
        }

        // Create suggestion from rule name
        if (!event.function_name.empty()) {
            event.suggestion = "Rule: " + event.function_name;
        }

        event.raw_output = content;
        event.structured_data = "tflint_json";

        events.push_back(event);
    }

    yyjson_doc_free(doc);
    return events;
}


} // namespace duckdb
