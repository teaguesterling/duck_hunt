#include "sqlfluff_json_parser.hpp"
#include "../../core/parser_registry.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool SqlfluffJSONParser::canParse(const std::string& content) const {
    // Quick heuristic checks before parsing
    if (content.empty() || content[0] != '[') {
        return false;
    }

    // Look for sqlfluff-specific JSON structure indicators
    if (content.find("\"filepath\"") == std::string::npos ||
        content.find("\"violations\"") == std::string::npos) {
        return false;
    }

    return isValidSqlfluffJSON(content);
}

bool SqlfluffJSONParser::isValidSqlfluffJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;

    if (yyjson_is_arr(root)) {
        size_t arr_size = yyjson_arr_size(root);
        if (arr_size > 0) {
            yyjson_val *first_elem = yyjson_arr_get_first(root);
            if (yyjson_is_obj(first_elem)) {
                // Check for sqlfluff structure: filepath, violations
                yyjson_val *filepath = yyjson_obj_get(first_elem, "filepath");
                yyjson_val *violations = yyjson_obj_get(first_elem, "violations");

                is_valid = (filepath && yyjson_is_str(filepath)) &&
                          (violations && yyjson_is_arr(violations));
            }
        } else {
            // Empty array is valid output (no files processed)
            is_valid = true;
        }
    }

    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> SqlfluffJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;

    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        throw IOException("Failed to parse sqlfluff JSON");
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        throw IOException("Invalid sqlfluff JSON: root is not an array");
    }

    // Parse each file entry
    size_t file_idx, file_max;
    yyjson_val *file_entry;
    int64_t event_id = 1;

    yyjson_arr_foreach(root, file_idx, file_max, file_entry) {
        if (!yyjson_is_obj(file_entry)) continue;

        // Get filepath
        yyjson_val *filepath = yyjson_obj_get(file_entry, "filepath");
        if (!filepath || !yyjson_is_str(filepath)) continue;

        std::string file_path = yyjson_get_str(filepath);

        // Get violations array
        yyjson_val *violations = yyjson_obj_get(file_entry, "violations");
        if (!violations || !yyjson_is_arr(violations)) continue;

        // Parse each violation
        size_t viol_idx, viol_max;
        yyjson_val *violation;

        yyjson_arr_foreach(violations, viol_idx, viol_max, violation) {
            if (!yyjson_is_obj(violation)) continue;

            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "sqlfluff";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.category = "sql_style";
            event.file_path = file_path;

            // Get line number
            yyjson_val *line_no = yyjson_obj_get(violation, "line_no");
            if (line_no && yyjson_is_int(line_no)) {
                event.line_number = yyjson_get_int(line_no);
            } else {
                event.line_number = -1;
            }

            // Get column position
            yyjson_val *line_pos = yyjson_obj_get(violation, "line_pos");
            if (line_pos && yyjson_is_int(line_pos)) {
                event.column_number = yyjson_get_int(line_pos);
            } else {
                event.column_number = -1;
            }

            // Get rule code as error_code
            yyjson_val *code = yyjson_obj_get(violation, "code");
            if (code && yyjson_is_str(code)) {
                event.error_code = yyjson_get_str(code);
            }

            // Get rule name for context
            yyjson_val *rule = yyjson_obj_get(violation, "rule");
            if (rule && yyjson_is_str(rule)) {
                event.function_name = yyjson_get_str(rule);
            }

            // Get description as message
            yyjson_val *description = yyjson_obj_get(violation, "description");
            if (description && yyjson_is_str(description)) {
                event.message = yyjson_get_str(description);
            }

            // All sqlfluff violations are warnings by default
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";

            // Create suggestion from rule name
            if (!event.function_name.empty()) {
                event.suggestion = "Rule: " + event.function_name;
            }

            event.raw_output = content;
            event.structured_data = "sqlfluff_json";

            events.push_back(event);
        }
    }

    yyjson_doc_free(doc);
    return events;
}

// Auto-register this parser
REGISTER_PARSER(SqlfluffJSONParser);

} // namespace duckdb
