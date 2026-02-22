#include "ruff_json_parser.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool RuffJsonParser::canParse(const std::string &content) const {
	// No content[0] gate: mixed-format content (CI logs before JSON) is common.
	// The find() checks below search the full string.
	if (content.empty()) {
		return false;
	}

	// Look for ruff-specific JSON structure indicators
	// Ruff has "code", "filename", "location", "message"
	if (content.find("\"code\"") == std::string::npos || content.find("\"filename\"") == std::string::npos ||
	    content.find("\"location\"") == std::string::npos) {
		return false;
	}

	// Also check for ruff-specific field "noqa_row" to distinguish from other linters
	if (content.find("\"noqa_row\"") == std::string::npos && content.find("\"url\"") == std::string::npos) {
		return false;
	}

	// Verify it's valid JSON with expected structure
	yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
	if (!doc) {
		return false;
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	bool is_valid = false;

	if (yyjson_is_arr(root)) {
		size_t arr_size = yyjson_arr_size(root);
		if (arr_size == 0) {
			// Empty array is valid ruff output (no errors)
			is_valid = true;
		} else {
			// Check first element has ruff structure
			yyjson_val *first = yyjson_arr_get_first(root);
			if (yyjson_is_obj(first)) {
				yyjson_val *code = yyjson_obj_get(first, "code");
				yyjson_val *filename = yyjson_obj_get(first, "filename");
				yyjson_val *location = yyjson_obj_get(first, "location");

				is_valid = (code && yyjson_is_str(code)) && (filename && yyjson_is_str(filename)) &&
				           (location && yyjson_is_obj(location));
			}
		}
	}

	yyjson_doc_free(doc);
	return is_valid;
}

std::vector<ValidationEvent> RuffJsonParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;

	yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
	if (!doc) {
		throw IOException("Failed to parse Ruff JSON");
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!yyjson_is_arr(root)) {
		yyjson_doc_free(doc);
		throw IOException("Invalid Ruff JSON: root is not an array");
	}

	size_t idx, max;
	yyjson_val *issue;
	int64_t event_id = 1;

	yyjson_arr_foreach(root, idx, max, issue) {
		if (!yyjson_is_obj(issue)) {
			continue;
		}

		ValidationEvent event;
		event.event_id = event_id++;
		event.tool_name = "ruff";
		event.event_type = ValidationEventType::LINT_ISSUE;
		event.category = "ruff_json";
		event.execution_time = 0.0;

		// Extract filename
		yyjson_val *filename = yyjson_obj_get(issue, "filename");
		if (filename && yyjson_is_str(filename)) {
			event.ref_file = yyjson_get_str(filename);
		}

		// Extract location (row/column)
		yyjson_val *location = yyjson_obj_get(issue, "location");
		if (location && yyjson_is_obj(location)) {
			yyjson_val *row = yyjson_obj_get(location, "row");
			yyjson_val *column = yyjson_obj_get(location, "column");

			if (row && yyjson_is_num(row)) {
				event.ref_line = yyjson_get_int(row);
			} else {
				event.ref_line = -1;
			}

			if (column && yyjson_is_num(column)) {
				event.ref_column = yyjson_get_int(column);
			} else {
				event.ref_column = -1;
			}
		}

		// Extract code (rule ID like F401, E501)
		yyjson_val *code = yyjson_obj_get(issue, "code");
		if (code && yyjson_is_str(code)) {
			event.error_code = yyjson_get_str(code);
			event.function_name = yyjson_get_str(code);

			// Determine severity based on rule code prefix
			std::string rule_code = event.error_code;
			if (!rule_code.empty()) {
				char prefix = rule_code[0];
				if (prefix == 'E' || prefix == 'F') {
					// E = pycodestyle errors, F = pyflakes
					event.severity = "error";
					event.status = ValidationEventStatus::ERROR;
				} else if (prefix == 'W') {
					// W = warnings
					event.severity = "warning";
					event.status = ValidationEventStatus::WARNING;
				} else {
					// Other rules (C, I, N, B, etc.)
					event.severity = "warning";
					event.status = ValidationEventStatus::WARNING;
				}
			}
		}

		// Extract message
		yyjson_val *message = yyjson_obj_get(issue, "message");
		if (message && yyjson_is_str(message)) {
			event.message = yyjson_get_str(message);
		}

		// Extract URL to rule documentation
		yyjson_val *url = yyjson_obj_get(issue, "url");
		if (url && yyjson_is_str(url)) {
			event.suggestion = yyjson_get_str(url);
		}

		// Check for fix availability
		yyjson_val *fix = yyjson_obj_get(issue, "fix");
		if (fix && yyjson_is_obj(fix)) {
			yyjson_val *fix_message = yyjson_obj_get(fix, "message");
			if (fix_message && yyjson_is_str(fix_message)) {
				if (event.suggestion.empty()) {
					event.suggestion = yyjson_get_str(fix_message);
				} else {
					event.suggestion += " | " + std::string(yyjson_get_str(fix_message));
				}
			}
		}

		events.push_back(event);
	}

	// Add summary event
	ValidationEvent summary;
	summary.event_id = event_id++;
	summary.event_type = ValidationEventType::SUMMARY;
	summary.tool_name = "ruff";
	summary.category = "ruff_json";
	summary.ref_file = "";
	summary.ref_line = -1;
	summary.ref_column = -1;
	summary.execution_time = 0.0;

	size_t issue_count = events.size();
	if (issue_count == 0) {
		summary.status = ValidationEventStatus::PASS;
		summary.severity = "info";
		summary.message = "All checks passed!";
	} else {
		summary.status = ValidationEventStatus::FAIL;
		summary.severity = "error";
		summary.message = "Found " + std::to_string(issue_count) + " error" + (issue_count != 1 ? "s" : "");
	}

	summary.structured_data = "{\"errors\":" + std::to_string(issue_count) + "}";
	events.push_back(summary);

	yyjson_doc_free(doc);
	return events;
}

} // namespace duckdb
