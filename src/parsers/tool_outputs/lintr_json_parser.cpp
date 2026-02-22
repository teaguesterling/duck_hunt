#include "lintr_json_parser.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool LintrJSONParser::canParse(const std::string &content) const {
	// No content[0] gate: mixed-format content (CI logs before JSON) is common.
	// The find() checks below search the full string.
	if (content.empty()) {
		return false;
	}

	// Look for lintr-specific JSON structure indicators
	if (content.find("\"filename\"") == std::string::npos || content.find("\"linter\"") == std::string::npos) {
		return false;
	}

	return isValidLintrJSON(content);
}

bool LintrJSONParser::isValidLintrJSON(const std::string &content) const {
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
				// Check for lintr structure: filename, line_number, linter, message, type
				yyjson_val *filename = yyjson_obj_get(first_elem, "filename");
				yyjson_val *linter = yyjson_obj_get(first_elem, "linter");
				yyjson_val *message = yyjson_obj_get(first_elem, "message");

				is_valid = (filename && yyjson_is_str(filename)) && (linter && yyjson_is_str(linter)) &&
				           (message && yyjson_is_str(message));
			}
		} else {
			// Empty array is valid output (no issues found)
			is_valid = true;
		}
	}

	yyjson_doc_free(doc);
	return is_valid;
}

std::vector<ValidationEvent> LintrJSONParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;

	yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
	if (!doc) {
		throw IOException("Failed to parse lintr JSON");
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!yyjson_is_arr(root)) {
		yyjson_doc_free(doc);
		throw IOException("Invalid lintr JSON: root is not an array");
	}

	size_t idx, max;
	yyjson_val *issue;
	int64_t event_id = 1;

	yyjson_arr_foreach(root, idx, max, issue) {
		if (!yyjson_is_obj(issue))
			continue;

		ValidationEvent event;
		event.event_id = event_id++;
		event.tool_name = "lintr";
		event.event_type = ValidationEventType::LINT_ISSUE;
		event.category = "r_code_style";

		// Get filename
		yyjson_val *filename = yyjson_obj_get(issue, "filename");
		if (filename && yyjson_is_str(filename)) {
			event.ref_file = yyjson_get_str(filename);
		}

		// Get line number
		yyjson_val *line_number = yyjson_obj_get(issue, "line_number");
		if (line_number && yyjson_is_int(line_number)) {
			event.ref_line = yyjson_get_int(line_number);
		} else {
			event.ref_line = -1;
		}

		// Get column number
		yyjson_val *column_number = yyjson_obj_get(issue, "column_number");
		if (column_number && yyjson_is_int(column_number)) {
			event.ref_column = yyjson_get_int(column_number);
		} else {
			event.ref_column = -1;
		}

		// Get linter name as error code
		yyjson_val *linter = yyjson_obj_get(issue, "linter");
		if (linter && yyjson_is_str(linter)) {
			event.error_code = yyjson_get_str(linter);
		}

		// Get message
		yyjson_val *message = yyjson_obj_get(issue, "message");
		if (message && yyjson_is_str(message)) {
			event.message = yyjson_get_str(message);
		}

		// Get type and map to status
		yyjson_val *type = yyjson_obj_get(issue, "type");
		if (type && yyjson_is_str(type)) {
			std::string type_str = yyjson_get_str(type);
			event.severity = type_str;

			// Map lintr types to ValidationEventStatus
			if (type_str == "error") {
				event.status = ValidationEventStatus::ERROR;
			} else if (type_str == "warning") {
				event.status = ValidationEventStatus::WARNING;
			} else if (type_str == "style") {
				event.status = ValidationEventStatus::WARNING;
			} else {
				event.status = ValidationEventStatus::INFO;
			}
		} else {
			event.severity = "style";
			event.status = ValidationEventStatus::WARNING;
		}

		// Get line content as suggestion (if available)
		yyjson_val *line_content = yyjson_obj_get(issue, "line");
		if (line_content && yyjson_is_str(line_content)) {
			event.suggestion = "Code: " + std::string(yyjson_get_str(line_content));
		}

		event.log_content = content;
		event.structured_data = "lintr_json";

		events.push_back(event);
	}

	yyjson_doc_free(doc);
	return events;
}

} // namespace duckdb
