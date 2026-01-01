#include "kubescore_json_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "yyjson.hpp"
#include <sstream>

namespace duckdb {

using namespace duckdb_yyjson;

bool KubeScoreJSONParser::canParse(const std::string &content) const {
	return isValidKubeScoreJSON(content);
}

bool KubeScoreJSONParser::isValidKubeScoreJSON(const std::string &content) const {
	// Quick structural checks for kube-score JSON
	if (content.empty() || content.find('[') == std::string::npos) {
		return false;
	}

	// Look for kube-score specific fields
	if (content.find("object_name") != std::string::npos && content.find("checks") != std::string::npos &&
	    content.find("grade") != std::string::npos) {
		return true;
	}

	return false;
}

ValidationEventStatus KubeScoreJSONParser::mapGradeToStatus(const std::string &grade) const {
	if (grade == "CRITICAL") {
		return ValidationEventStatus::ERROR;
	} else if (grade == "WARNING") {
		return ValidationEventStatus::WARNING;
	} else {
		return ValidationEventStatus::INFO;
	}
}

std::vector<ValidationEvent> KubeScoreJSONParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;

	// Parse JSON using yyjson
	yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
	if (!doc) {
		throw IOException("Failed to parse kube-score JSON");
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!yyjson_is_arr(root)) {
		yyjson_doc_free(doc);
		throw IOException("Invalid kube-score JSON: root is not an array");
	}

	// Parse each Kubernetes object
	size_t obj_idx, obj_max;
	yyjson_val *k8s_object;
	int64_t event_id = 1;

	yyjson_arr_foreach(root, obj_idx, obj_max, k8s_object) {
		if (!yyjson_is_obj(k8s_object))
			continue;

		// Get object metadata
		std::string object_name;
		std::string file_name;
		std::string resource_kind;
		std::string namespace_name = "default";
		int line_number = -1;

		yyjson_val *obj_name = yyjson_obj_get(k8s_object, "object_name");
		if (obj_name && yyjson_is_str(obj_name)) {
			object_name = yyjson_get_str(obj_name);
		}

		yyjson_val *file_name_val = yyjson_obj_get(k8s_object, "file_name");
		if (file_name_val && yyjson_is_str(file_name_val)) {
			file_name = yyjson_get_str(file_name_val);
		}

		yyjson_val *file_row = yyjson_obj_get(k8s_object, "file_row");
		if (file_row && yyjson_is_int(file_row)) {
			line_number = yyjson_get_int(file_row);
		}

		// Get resource kind from type_meta
		yyjson_val *type_meta = yyjson_obj_get(k8s_object, "type_meta");
		if (type_meta && yyjson_is_obj(type_meta)) {
			yyjson_val *kind = yyjson_obj_get(type_meta, "kind");
			if (kind && yyjson_is_str(kind)) {
				resource_kind = yyjson_get_str(kind);
			}
		}

		// Get namespace from object_meta
		yyjson_val *object_meta = yyjson_obj_get(k8s_object, "object_meta");
		if (object_meta && yyjson_is_obj(object_meta)) {
			yyjson_val *ns = yyjson_obj_get(object_meta, "namespace");
			if (ns && yyjson_is_str(ns)) {
				namespace_name = yyjson_get_str(ns);
			}
		}

		// Get checks array
		yyjson_val *checks = yyjson_obj_get(k8s_object, "checks");
		if (!checks || !yyjson_is_arr(checks))
			continue;

		// Parse each check
		size_t check_idx, check_max;
		yyjson_val *check;

		yyjson_arr_foreach(checks, check_idx, check_max, check) {
			if (!yyjson_is_obj(check))
				continue;

			// Get grade to determine if this is an issue
			yyjson_val *grade = yyjson_obj_get(check, "grade");
			if (!grade || !yyjson_is_str(grade))
				continue;

			std::string grade_str = yyjson_get_str(grade);

			// Skip OK checks unless they have comments
			yyjson_val *comments = yyjson_obj_get(check, "comments");
			bool has_comments = comments && yyjson_is_arr(comments) && yyjson_arr_size(comments) > 0;

			if (grade_str == "OK" && !has_comments)
				continue;

			// Get check information
			yyjson_val *check_info = yyjson_obj_get(check, "check");
			std::string check_id;
			std::string check_name;
			std::string check_comment;

			if (check_info && yyjson_is_obj(check_info)) {
				yyjson_val *id = yyjson_obj_get(check_info, "id");
				if (id && yyjson_is_str(id)) {
					check_id = yyjson_get_str(id);
				}

				yyjson_val *name = yyjson_obj_get(check_info, "name");
				if (name && yyjson_is_str(name)) {
					check_name = yyjson_get_str(name);
				}

				yyjson_val *comment = yyjson_obj_get(check_info, "comment");
				if (comment && yyjson_is_str(comment)) {
					check_comment = yyjson_get_str(comment);
				}
			}

			// Create validation event for each comment or one general event
			if (has_comments) {
				size_t comment_idx, comment_max;
				yyjson_val *comment_obj;

				yyjson_arr_foreach(comments, comment_idx, comment_max, comment_obj) {
					if (!yyjson_is_obj(comment_obj))
						continue;

					ValidationEvent event;
					event.event_id = event_id++;
					event.tool_name = "kube-score";
					event.event_type = ValidationEventType::LINT_ISSUE;
					event.category = "kubernetes";
					event.ref_file = file_name;
					event.ref_line = line_number;
					event.ref_column = -1;
					event.error_code = check_id;
					event.function_name = object_name + " (" + resource_kind + ")";

					// Map grade to status and severity
					event.status = mapGradeToStatus(grade_str);
					if (grade_str == "CRITICAL") {
						event.severity = "critical";
					} else if (grade_str == "WARNING") {
						event.severity = "warning";
					} else {
						event.severity = "info";
					}

					// Get comment details
					yyjson_val *summary = yyjson_obj_get(comment_obj, "summary");
					yyjson_val *description = yyjson_obj_get(comment_obj, "description");
					yyjson_val *path = yyjson_obj_get(comment_obj, "path");

					if (summary && yyjson_is_str(summary)) {
						event.message = yyjson_get_str(summary);
					} else {
						event.message = check_name;
					}

					if (description && yyjson_is_str(description)) {
						event.suggestion = yyjson_get_str(description);
					}

					// Add path context if available
					if (path && yyjson_is_str(path) && strlen(yyjson_get_str(path)) > 0) {
						event.test_name = yyjson_get_str(path);
					}

					event.execution_time = 0.0;
					event.log_content = content;
					event.structured_data = "kube_score_json";

					events.push_back(event);
				}
			} else {
				// Create general event for non-OK checks without specific comments
				ValidationEvent event;
				event.event_id = event_id++;
				event.tool_name = "kube-score";
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.category = "kubernetes";
				event.ref_file = file_name;
				event.ref_line = line_number;
				event.ref_column = -1;
				event.error_code = check_id;
				event.function_name = object_name + " (" + resource_kind + ")";
				event.message = check_name;
				event.suggestion = check_comment;

				// Map grade to status and severity
				event.status = mapGradeToStatus(grade_str);
				if (grade_str == "CRITICAL") {
					event.severity = "critical";
				} else if (grade_str == "WARNING") {
					event.severity = "warning";
				} else {
					event.severity = "info";
				}

				event.execution_time = 0.0;
				event.log_content = content;
				event.structured_data = "kube_score_json";

				events.push_back(event);
			}
		}
	}

	yyjson_doc_free(doc);
	return events;
}

} // namespace duckdb
