#include "config_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "yyjson.hpp"
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace duckdb {

using namespace duckdb_yyjson;

// Pattern to extract named group names from user patterns (same as regexp_parser)
static const std::regex RE_NAME_EXTRACTOR(R"(\(\?(?:P)?<([a-zA-Z_][a-zA-Z0-9_]*)>)");
// Pattern to strip named group syntax for std::regex compatibility
static const std::regex RE_NAMED_GROUP_STRIP(R"(\(\?P?<[a-zA-Z_][a-zA-Z0-9_]*>)");

ValidationEventType ParseEventType(const std::string &event_type_str) {
	if (event_type_str == "BUILD_ERROR") {
		return ValidationEventType::BUILD_ERROR;
	} else if (event_type_str == "LINT_ISSUE") {
		return ValidationEventType::LINT_ISSUE;
	} else if (event_type_str == "TEST_RESULT") {
		return ValidationEventType::TEST_RESULT;
	} else if (event_type_str == "TYPE_ERROR") {
		return ValidationEventType::TYPE_ERROR;
	} else if (event_type_str == "SECURITY_FINDING") {
		return ValidationEventType::SECURITY_FINDING;
	} else if (event_type_str == "MEMORY_ERROR") {
		return ValidationEventType::MEMORY_ERROR;
	} else if (event_type_str == "UNKNOWN") {
		return ValidationEventType::UNKNOWN;
	} else {
		throw std::runtime_error(
		    "Invalid event_type: " + event_type_str +
		    ". Valid types: BUILD_ERROR, LINT_ISSUE, TEST_RESULT, TYPE_ERROR, SECURITY_FINDING, MEMORY_ERROR, UNKNOWN");
	}
}

std::vector<std::string> ExtractGroupNames(const std::string &pattern) {
	std::vector<std::string> group_names;
	std::string::const_iterator search_start = pattern.cbegin();
	std::smatch name_match;

	while (std::regex_search(search_start, pattern.cend(), name_match, RE_NAME_EXTRACTOR)) {
		group_names.push_back(name_match[1].str());
		search_start = name_match.suffix().first;
	}
	return group_names;
}

std::regex CompilePatternWithNamedGroups(const std::string &pattern) {
	// Convert Python-style named groups to regular groups
	std::string modified_pattern = std::regex_replace(pattern, RE_NAMED_GROUP_STRIP, "(");
	return std::regex(modified_pattern);
}

unique_ptr<ConfigBasedParser> ConfigBasedParser::FromJson(const std::string &json_config) {
	// Parse JSON using yyjson
	yyjson_doc *doc = yyjson_read(json_config.c_str(), json_config.size(), 0);
	if (!doc) {
		throw std::runtime_error("Invalid JSON");
	}

	// RAII cleanup
	struct DocGuard {
		yyjson_doc *doc;
		~DocGuard() {
			if (doc)
				yyjson_doc_free(doc);
		}
	} doc_guard {doc};

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!yyjson_is_obj(root)) {
		throw std::runtime_error("Config must be a JSON object");
	}

	// Required field: name
	yyjson_val *name_val = yyjson_obj_get(root, "name");
	if (!name_val || !yyjson_is_str(name_val)) {
		throw std::runtime_error("Missing required field: name");
	}
	std::string format_name = yyjson_get_str(name_val);

	// Required field: patterns
	yyjson_val *patterns_val = yyjson_obj_get(root, "patterns");
	if (!patterns_val || !yyjson_is_arr(patterns_val)) {
		throw std::runtime_error("Missing required field: patterns");
	}

	// Optional fields
	std::string display_name = format_name;
	yyjson_val *display_name_val = yyjson_obj_get(root, "display_name");
	if (display_name_val && yyjson_is_str(display_name_val)) {
		display_name = yyjson_get_str(display_name_val);
	}

	std::string tool_name = format_name;
	yyjson_val *tool_name_val = yyjson_obj_get(root, "tool_name");
	if (tool_name_val && yyjson_is_str(tool_name_val)) {
		tool_name = yyjson_get_str(tool_name_val);
	}

	std::string category = "tool_output";
	yyjson_val *category_val = yyjson_obj_get(root, "category");
	if (category_val && yyjson_is_str(category_val)) {
		category = yyjson_get_str(category_val);
	}

	std::string description;
	yyjson_val *desc_val = yyjson_obj_get(root, "description");
	if (desc_val && yyjson_is_str(desc_val)) {
		description = yyjson_get_str(desc_val);
	}

	int priority = 50;
	yyjson_val *priority_val = yyjson_obj_get(root, "priority");
	if (priority_val && yyjson_is_int(priority_val)) {
		priority = yyjson_get_int(priority_val);
	}

	// Aliases
	std::vector<std::string> aliases;
	yyjson_val *aliases_val = yyjson_obj_get(root, "aliases");
	if (aliases_val && yyjson_is_arr(aliases_val)) {
		size_t idx, max;
		yyjson_val *val;
		yyjson_arr_foreach(aliases_val, idx, max, val) {
			if (yyjson_is_str(val)) {
				aliases.push_back(yyjson_get_str(val));
			}
		}
	}

	// Groups
	std::vector<std::string> groups;
	yyjson_val *groups_val = yyjson_obj_get(root, "groups");
	if (groups_val && yyjson_is_arr(groups_val)) {
		size_t idx, max;
		yyjson_val *val;
		yyjson_arr_foreach(groups_val, idx, max, val) {
			if (yyjson_is_str(val)) {
				groups.push_back(yyjson_get_str(val));
			}
		}
	}
	if (groups.empty()) {
		groups.push_back("custom");
	}

	// Detection
	ConfigDetection detection;
	yyjson_val *detection_val = yyjson_obj_get(root, "detection");
	if (detection_val && yyjson_is_obj(detection_val)) {
		// contains (any)
		yyjson_val *contains_val = yyjson_obj_get(detection_val, "contains");
		if (contains_val && yyjson_is_arr(contains_val)) {
			size_t idx, max;
			yyjson_val *val;
			yyjson_arr_foreach(contains_val, idx, max, val) {
				if (yyjson_is_str(val)) {
					detection.contains.push_back(yyjson_get_str(val));
				}
			}
		}

		// contains_all
		yyjson_val *contains_all_val = yyjson_obj_get(detection_val, "contains_all");
		if (contains_all_val && yyjson_is_arr(contains_all_val)) {
			size_t idx, max;
			yyjson_val *val;
			yyjson_arr_foreach(contains_all_val, idx, max, val) {
				if (yyjson_is_str(val)) {
					detection.contains_all.push_back(yyjson_get_str(val));
				}
			}
		}

		// regex
		yyjson_val *regex_val = yyjson_obj_get(detection_val, "regex");
		if (regex_val && yyjson_is_str(regex_val)) {
			detection.regex_pattern = yyjson_get_str(regex_val);
			try {
				detection.compiled_regex = std::regex(detection.regex_pattern);
				detection.has_regex = true;
			} catch (const std::regex_error &e) {
				throw std::runtime_error("Invalid detection regex: " + std::string(e.what()));
			}
		}
	}

	// Patterns
	std::vector<ConfigPattern> patterns;
	size_t pattern_idx, pattern_max;
	yyjson_val *pattern_val;
	yyjson_arr_foreach(patterns_val, pattern_idx, pattern_max, pattern_val) {
		if (!yyjson_is_obj(pattern_val)) {
			throw std::runtime_error("Pattern must be an object");
		}

		ConfigPattern pattern;

		// Optional name
		yyjson_val *pattern_name_val = yyjson_obj_get(pattern_val, "name");
		if (pattern_name_val && yyjson_is_str(pattern_name_val)) {
			pattern.name = yyjson_get_str(pattern_name_val);
		}

		// Required: regex
		yyjson_val *regex_val = yyjson_obj_get(pattern_val, "regex");
		if (!regex_val || !yyjson_is_str(regex_val)) {
			throw std::runtime_error("Pattern missing required field: regex");
		}
		pattern.original_pattern = yyjson_get_str(regex_val);

		try {
			pattern.group_names = ExtractGroupNames(pattern.original_pattern);
			pattern.compiled_regex = CompilePatternWithNamedGroups(pattern.original_pattern);
		} catch (const std::regex_error &e) {
			throw std::runtime_error("Invalid regex pattern '" + pattern.original_pattern + "': " + e.what());
		}

		// Required: event_type
		yyjson_val *event_type_val = yyjson_obj_get(pattern_val, "event_type");
		if (!event_type_val || !yyjson_is_str(event_type_val)) {
			throw std::runtime_error("Pattern missing required field: event_type");
		}
		pattern.event_type = ParseEventType(yyjson_get_str(event_type_val));

		// Optional: severity (fixed)
		yyjson_val *severity_val = yyjson_obj_get(pattern_val, "severity");
		if (severity_val && yyjson_is_str(severity_val)) {
			pattern.fixed_severity = yyjson_get_str(severity_val);
		}

		// Optional: severity_map
		yyjson_val *severity_map_val = yyjson_obj_get(pattern_val, "severity_map");
		if (severity_map_val && yyjson_is_obj(severity_map_val)) {
			yyjson_obj_iter iter;
			yyjson_obj_iter_init(severity_map_val, &iter);
			yyjson_val *key;
			while ((key = yyjson_obj_iter_next(&iter))) {
				yyjson_val *val = yyjson_obj_iter_get_val(key);
				if (yyjson_is_str(val)) {
					pattern.severity_map[yyjson_get_str(key)] = yyjson_get_str(val);
				}
			}
		}

		// Optional: status_map
		yyjson_val *status_map_val = yyjson_obj_get(pattern_val, "status_map");
		if (status_map_val && yyjson_is_obj(status_map_val)) {
			yyjson_obj_iter iter;
			yyjson_obj_iter_init(status_map_val, &iter);
			yyjson_val *key;
			while ((key = yyjson_obj_iter_next(&iter))) {
				yyjson_val *val = yyjson_obj_iter_get_val(key);
				if (yyjson_is_str(val)) {
					pattern.status_map[yyjson_get_str(key)] = yyjson_get_str(val);
				}
			}
		}

		patterns.push_back(std::move(pattern));
	}

	if (patterns.empty()) {
		throw std::runtime_error("At least one pattern is required");
	}

	return make_uniq<ConfigBasedParser>(format_name, display_name, category, description, tool_name, priority, aliases,
	                                    groups, std::move(detection), std::move(patterns));
}

ConfigBasedParser::ConfigBasedParser(std::string format_name, std::string display_name, std::string category,
                                     std::string description, std::string tool_name, int priority,
                                     std::vector<std::string> aliases, std::vector<std::string> groups,
                                     ConfigDetection detection, std::vector<ConfigPattern> patterns)
    : format_name_(std::move(format_name)), display_name_(std::move(display_name)), category_(std::move(category)),
      description_(std::move(description)), tool_name_(std::move(tool_name)), priority_(priority),
      aliases_(std::move(aliases)), groups_(std::move(groups)), detection_(std::move(detection)),
      patterns_(std::move(patterns)) {
}

bool ConfigBasedParser::canParse(const std::string &content) const {
	// If no detection rules, only match when explicitly requested
	if (detection_.contains.empty() && detection_.contains_all.empty() && !detection_.has_regex) {
		return false;
	}

	// Check contains (any) - at least one must be present
	if (!detection_.contains.empty()) {
		bool found_any = false;
		for (const auto &marker : detection_.contains) {
			if (content.find(marker) != std::string::npos) {
				found_any = true;
				break;
			}
		}
		if (!found_any) {
			return false;
		}
	}

	// Check contains_all - all must be present
	for (const auto &marker : detection_.contains_all) {
		if (content.find(marker) == std::string::npos) {
			return false;
		}
	}

	// Check regex
	if (detection_.has_regex) {
		if (!std::regex_search(content, detection_.compiled_regex)) {
			return false;
		}
	}

	return true;
}

std::string ConfigBasedParser::getGroupValue(const std::smatch &match, const std::vector<std::string> &group_names,
                                             const std::vector<std::string> &target_names) {
	for (const auto &target : target_names) {
		for (size_t i = 0; i < group_names.size(); i++) {
			if (group_names[i] == target && (i + 1) < match.size() && match[i + 1].matched) {
				return match[i + 1].str();
			}
		}
	}
	return "";
}

std::vector<ValidationEvent> ConfigBasedParser::parseLineInternal(const std::string &line, int32_t line_number,
                                                                  int64_t &event_id) const {
	std::vector<ValidationEvent> events;

	// Try each pattern in order (first match wins)
	for (const auto &pattern : patterns_) {
		std::smatch match;
		if (std::regex_search(line, match, pattern.compiled_regex)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = tool_name_;
			event.event_type = pattern.event_type;
			event.category = category_;
			event.log_content = line;
			event.log_line_start = line_number;
			event.log_line_end = line_number;

			// Extract message
			std::string message = getGroupValue(match, pattern.group_names, {"message", "msg"});
			if (!message.empty()) {
				event.message = message;
			} else {
				event.message = match[0].str();
			}

			// Handle severity
			std::string severity_val = getGroupValue(match, pattern.group_names, {"severity", "level"});
			std::string status_val = getGroupValue(match, pattern.group_names, {"status", "result"});

			// Check status_map first (for TEST_RESULT)
			if (!status_val.empty() && !pattern.status_map.empty()) {
				auto status_it = pattern.status_map.find(status_val);
				if (status_it != pattern.status_map.end()) {
					std::string mapped_status = status_it->second;
					if (mapped_status == "PASS") {
						event.status = ValidationEventStatus::PASS;
						event.severity = "info";
					} else if (mapped_status == "FAIL") {
						event.status = ValidationEventStatus::FAIL;
						event.severity = "error";
					} else if (mapped_status == "SKIP") {
						event.status = ValidationEventStatus::SKIP;
						event.severity = "info";
					}
				}
			} else if (!severity_val.empty() && !pattern.severity_map.empty()) {
				// Check severity_map
				auto sev_it = pattern.severity_map.find(severity_val);
				if (sev_it != pattern.severity_map.end()) {
					event.severity = sev_it->second;
					// Set status based on severity
					if (event.severity == "error" || event.severity == "critical") {
						event.status = ValidationEventStatus::ERROR;
					} else if (event.severity == "warning") {
						event.status = ValidationEventStatus::WARNING;
					} else {
						event.status = ValidationEventStatus::INFO;
					}
				}
			} else if (!pattern.fixed_severity.empty()) {
				// Use fixed severity
				event.severity = pattern.fixed_severity;
				if (event.severity == "error" || event.severity == "critical") {
					event.status = ValidationEventStatus::ERROR;
				} else if (event.severity == "warning") {
					event.status = ValidationEventStatus::WARNING;
				} else {
					event.status = ValidationEventStatus::INFO;
				}
			} else if (!severity_val.empty()) {
				// Use captured severity directly
				std::string sev_upper = severity_val;
				std::transform(sev_upper.begin(), sev_upper.end(), sev_upper.begin(), ::toupper);
				std::string sev_lower = severity_val;
				std::transform(sev_lower.begin(), sev_lower.end(), sev_lower.begin(), ::tolower);

				// Check if this looks like a test status (PASS/FAIL/SKIP) rather than severity
				if (sev_upper == "PASS" || sev_upper == "PASSED" || sev_upper == "OK") {
					event.status = ValidationEventStatus::PASS;
					event.severity = "info";
				} else if (sev_upper == "FAIL" || sev_upper == "FAILED") {
					event.status = ValidationEventStatus::FAIL;
					event.severity = "error";
				} else if (sev_upper == "SKIP" || sev_upper == "SKIPPED") {
					event.status = ValidationEventStatus::SKIP;
					event.severity = "info";
				} else if (sev_lower == "error" || sev_lower == "fatal") {
					event.status = ValidationEventStatus::ERROR;
					event.severity = "error";
				} else if (sev_lower == "warning" || sev_lower == "warn") {
					event.status = ValidationEventStatus::WARNING;
					event.severity = "warning";
				} else if (sev_lower == "info" || sev_lower == "note" || sev_lower == "debug") {
					event.status = ValidationEventStatus::INFO;
					event.severity = sev_lower;
				} else if (sev_lower == "critical") {
					event.status = ValidationEventStatus::ERROR;
					event.severity = "critical";
				} else {
					// Unknown value - treat as info severity
					event.status = ValidationEventStatus::INFO;
					event.severity = sev_lower;
				}
			} else {
				// Default based on event type
				if (pattern.event_type == ValidationEventType::BUILD_ERROR) {
					event.status = ValidationEventStatus::ERROR;
					event.severity = "error";
				} else if (pattern.event_type == ValidationEventType::TEST_RESULT) {
					// Check for PASS/FAIL in captured value
					if (!status_val.empty()) {
						std::string status_upper = status_val;
						std::transform(status_upper.begin(), status_upper.end(), status_upper.begin(), ::toupper);
						if (status_upper == "PASS" || status_upper == "OK" || status_upper == "PASSED") {
							event.status = ValidationEventStatus::PASS;
							event.severity = "info";
						} else if (status_upper == "FAIL" || status_upper == "FAILED" || status_upper == "ERROR") {
							event.status = ValidationEventStatus::FAIL;
							event.severity = "error";
						} else if (status_upper == "SKIP" || status_upper == "SKIPPED") {
							event.status = ValidationEventStatus::SKIP;
							event.severity = "info";
						}
					} else {
						event.status = ValidationEventStatus::INFO;
						event.severity = "info";
					}
				} else {
					event.status = ValidationEventStatus::INFO;
					event.severity = "info";
				}
			}

			// Extract file location
			std::string file_path = getGroupValue(match, pattern.group_names, {"file", "file_path", "path"});
			if (!file_path.empty()) {
				event.ref_file = file_path;
			}

			std::string line_str = getGroupValue(match, pattern.group_names, {"line", "lineno", "line_number"});
			if (!line_str.empty()) {
				try {
					event.ref_line = SafeParsing::SafeStoi(line_str);
				} catch (...) {
					event.ref_line = line_number;
				}
			} else {
				event.ref_line = line_number;
			}

			std::string col_str = getGroupValue(match, pattern.group_names, {"column", "col"});
			if (!col_str.empty()) {
				try {
					event.ref_column = SafeParsing::SafeStoi(col_str);
				} catch (...) {
					event.ref_column = -1;
				}
			} else {
				event.ref_column = -1;
			}

			// Extract error code
			std::string error_code = getGroupValue(match, pattern.group_names, {"error_code", "code", "rule"});
			if (!error_code.empty()) {
				event.error_code = error_code;
			}

			// Extract function name
			std::string func_name = getGroupValue(match, pattern.group_names, {"function_name", "func", "function"});
			if (!func_name.empty()) {
				event.function_name = func_name;
			}

			// Extract test name
			std::string test_name = getGroupValue(match, pattern.group_names, {"test_name", "test"});
			if (!test_name.empty()) {
				event.test_name = test_name;
			}

			// Extract hierarchical context
			std::string scope = getGroupValue(match, pattern.group_names, {"scope"});
			if (!scope.empty()) {
				event.scope = scope;
			}

			std::string group = getGroupValue(match, pattern.group_names, {"group"});
			if (!group.empty()) {
				event.group = group;
			}

			std::string unit = getGroupValue(match, pattern.group_names, {"unit"});
			if (!unit.empty()) {
				event.unit = unit;
			}

			events.push_back(event);
			break; // First match wins
		}
	}

	return events;
}

std::vector<ValidationEvent> ConfigBasedParser::parseLine(const std::string &line, int32_t line_number,
                                                          int64_t &event_id) const {
	return parseLineInternal(line, line_number, event_id);
}

std::vector<ValidationEvent> ConfigBasedParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;

	// Normalize line endings
	std::string normalized = SafeParsing::NormalizeLineEndings(content);

	// Parse line by line
	SafeParsing::SafeLineReader reader(normalized);
	std::string line;
	int64_t event_id = 1;

	while (reader.getLine(line)) {
		int line_num = reader.lineNumber();
		auto line_events = parseLineInternal(line, line_num, event_id);
		events.insert(events.end(), line_events.begin(), line_events.end());
	}

	return events;
}

} // namespace duckdb
