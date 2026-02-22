#include "hadolint_json_parser.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool HadolintJSONParser::canParse(const std::string &content) const {
	// No content[0] gate: mixed-format content (CI logs before JSON) is common.
	// The find() checks below search the full string.
	if (content.empty()) {
		return false;
	}

	// Look for hadolint-specific JSON structure indicators
	if (content.find("\"code\"") == std::string::npos || content.find("\"level\"") == std::string::npos) {
		return false;
	}

	// Check for Dockerfile-related content or DL codes
	if (content.find("Dockerfile") == std::string::npos && content.find("\"DL") == std::string::npos &&
	    content.find("\"SC") == std::string::npos) {
		return false;
	}

	return isValidHadolintJSON(content);
}

bool HadolintJSONParser::isValidHadolintJSON(const std::string &content) const {
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
				// Check for hadolint structure: file, line, code, message, level
				yyjson_val *code = yyjson_obj_get(first_elem, "code");
				yyjson_val *level = yyjson_obj_get(first_elem, "level");
				yyjson_val *message = yyjson_obj_get(first_elem, "message");

				is_valid = (code && yyjson_is_str(code)) && (level && yyjson_is_str(level)) &&
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

std::vector<ValidationEvent> HadolintJSONParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;

	yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
	if (!doc) {
		throw IOException("Failed to parse hadolint JSON");
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!yyjson_is_arr(root)) {
		yyjson_doc_free(doc);
		throw IOException("Invalid hadolint JSON: root is not an array");
	}

	size_t idx, max;
	yyjson_val *issue;
	int64_t event_id = 1;

	yyjson_arr_foreach(root, idx, max, issue) {
		if (!yyjson_is_obj(issue))
			continue;

		ValidationEvent event;
		event.event_id = event_id++;
		event.tool_name = "hadolint";
		event.event_type = ValidationEventType::LINT_ISSUE;
		event.category = "dockerfile";

		// Get file path
		yyjson_val *file = yyjson_obj_get(issue, "file");
		if (file && yyjson_is_str(file)) {
			event.ref_file = yyjson_get_str(file);
		}

		// Get line number
		yyjson_val *line = yyjson_obj_get(issue, "line");
		if (line && yyjson_is_int(line)) {
			event.ref_line = yyjson_get_int(line);
		} else {
			event.ref_line = -1;
		}

		// Get column number
		yyjson_val *column = yyjson_obj_get(issue, "column");
		if (column && yyjson_is_int(column)) {
			event.ref_column = yyjson_get_int(column);
		} else {
			event.ref_column = -1;
		}

		// Get code as error code
		yyjson_val *code = yyjson_obj_get(issue, "code");
		if (code && yyjson_is_str(code)) {
			event.error_code = yyjson_get_str(code);
		}

		// Get message
		yyjson_val *message = yyjson_obj_get(issue, "message");
		if (message && yyjson_is_str(message)) {
			event.message = yyjson_get_str(message);
		}

		// Get level and map to status
		yyjson_val *level = yyjson_obj_get(issue, "level");
		if (level && yyjson_is_str(level)) {
			std::string level_str = yyjson_get_str(level);
			event.severity = level_str;

			// Map hadolint levels to ValidationEventStatus
			if (level_str == "error") {
				event.status = ValidationEventStatus::ERROR;
			} else if (level_str == "warning") {
				event.status = ValidationEventStatus::WARNING;
			} else if (level_str == "info") {
				event.status = ValidationEventStatus::INFO;
			} else if (level_str == "style") {
				event.status = ValidationEventStatus::WARNING;
			} else {
				event.status = ValidationEventStatus::WARNING;
			}
		} else {
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
		}

		event.log_content = content;
		event.structured_data = "hadolint_json";

		events.push_back(event);
	}

	yyjson_doc_free(doc);
	return events;
}

} // namespace duckdb
