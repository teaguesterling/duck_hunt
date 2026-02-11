#include "regexp_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>
#include <string>
#include <map>
#include <algorithm>

namespace duckdb {

void RegexpParser::Parse(const std::string &content, const std::string &pattern,
                         std::vector<ValidationEvent> &events, bool include_unparsed) const {
	ParseWithRegexp(content, pattern, events, include_unparsed);
}

void RegexpParser::ParseWithRegexp(const std::string &content, const std::string &pattern,
                                   std::vector<ValidationEvent> &events, bool include_unparsed) {
	// Normalize line endings (CRLF -> LF, CR -> LF) before processing
	// This handles Windows (\r\n), Unix (\n), and old Mac (\r) line endings uniformly
	std::string normalized_content = SafeParsing::NormalizeLineEndings(content);

	// Extract named group names from the pattern
	// Supports both Python-style (?P<name>...) and ECMAScript-style (?<name>...)
	std::vector<std::string> group_names;
	std::regex name_extractor(R"(\(\?(?:P)?<([a-zA-Z_][a-zA-Z0-9_]*)>)");
	std::string::const_iterator search_start = pattern.cbegin();
	std::smatch name_match;

	while (std::regex_search(search_start, pattern.cend(), name_match, name_extractor)) {
		group_names.push_back(name_match[1].str());
		search_start = name_match.suffix().first;
	}

	// Convert Python-style named groups to std::regex compatible format
	// (?P<name>...) -> (?:...) but we keep track of positions
	std::string modified_pattern = std::regex_replace(pattern, std::regex(R"(\(\?P<([a-zA-Z_][a-zA-Z0-9_]*)>)"), "(?:");
	// Also handle ECMAScript-style (?<name>...) -> (?:...)
	modified_pattern = std::regex_replace(modified_pattern, std::regex(R"(\(\?<([a-zA-Z_][a-zA-Z0-9_]*)>)"), "(?:");

	// Actually, std::regex in C++11/14 doesn't support named groups, so we need to use indexed groups.
	// We'll convert named groups to regular groups and track which index corresponds to which name.
	// (?P<name>...) and (?<name>...) become (...)
	modified_pattern = std::regex_replace(pattern, std::regex(R"(\(\?P?<[a-zA-Z_][a-zA-Z0-9_]*>)"), "(");

	std::regex user_regex;
	try {
		user_regex = std::regex(modified_pattern);
	} catch (const std::regex_error &e) {
		// If regex compilation fails, create an error event
		ValidationEvent error_event;
		error_event.event_id = 1;
		error_event.tool_name = "regexp";
		error_event.event_type = ValidationEventType::BUILD_ERROR;
		error_event.status = ValidationEventStatus::ERROR;
		error_event.severity = "error";
		error_event.category = "parse_error";
		error_event.message = std::string("Invalid regex pattern: ") + e.what();
		error_event.ref_line = -1;
		error_event.ref_column = -1;
		events.push_back(error_event);
		return;
	}

	// Build a map from group name to index
	std::map<std::string, size_t> name_to_index;
	for (size_t i = 0; i < group_names.size(); i++) {
		name_to_index[group_names[i]] = i + 1; // Groups are 1-indexed
	}

	// Helper function to get group value by name
	auto getGroupValue = [&](const std::smatch &match, const std::vector<std::string> &names) -> std::string {
		for (const auto &name : names) {
			auto it = name_to_index.find(name);
			if (it != name_to_index.end() && it->second < match.size() && match[it->second].matched) {
				return match[it->second].str();
			}
		}
		return "";
	};

	// Parse content line by line
	std::istringstream stream(normalized_content);
	std::string line;
	int64_t event_id = 1;
	int line_num = 0;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		if (std::regex_search(line, match, user_regex)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "regexp";
			event.event_type = ValidationEventType::LINT_ISSUE;

			// Map captured groups to event fields
			std::string severity = getGroupValue(match, {"severity", "level"});
			if (!severity.empty()) {
				std::string severity_lower = severity;
				std::transform(severity_lower.begin(), severity_lower.end(), severity_lower.begin(), ::tolower);

				if (severity_lower == "error" || severity_lower == "fatal" || severity_lower == "fail" ||
				    severity_lower == "failed") {
					event.status = ValidationEventStatus::ERROR;
					event.severity = "error";
				} else if (severity_lower == "warning" || severity_lower == "warn") {
					event.status = ValidationEventStatus::WARNING;
					event.severity = "warning";
				} else if (severity_lower == "info" || severity_lower == "note" || severity_lower == "debug") {
					event.status = ValidationEventStatus::INFO;
					event.severity = "info";
				} else {
					event.status = ValidationEventStatus::WARNING;
					event.severity = severity;
				}
			} else {
				event.status = ValidationEventStatus::WARNING;
				event.severity = "warning";
			}

			// Message field
			std::string message = getGroupValue(match, {"message", "msg", "description", "text"});
			if (!message.empty()) {
				event.message = message;
			} else {
				// If no message group, use the entire matched line
				event.message = match[0].str();
			}

			// File path
			std::string file_path = getGroupValue(match, {"file", "file_path", "path", "filename"});
			if (!file_path.empty()) {
				event.ref_file = file_path;
			}

			// Line number
			std::string line_str = getGroupValue(match, {"line", "line_number", "lineno", "line_num"});
			if (!line_str.empty()) {
				try {
					event.ref_line = std::stoi(line_str);
				} catch (...) {
					event.ref_line = line_num; // Fall back to input line number
				}
			} else {
				event.ref_line = line_num;
			}

			// Column number
			std::string col_str = getGroupValue(match, {"column", "col", "ref_column", "colno"});
			if (!col_str.empty()) {
				try {
					event.ref_column = std::stoi(col_str);
				} catch (...) {
					event.ref_column = -1;
				}
			} else {
				event.ref_column = -1;
			}

			// Error code
			std::string error_code = getGroupValue(match, {"code", "error_code", "rule", "rule_id"});
			if (!error_code.empty()) {
				event.error_code = error_code;
			}

			// Category
			std::string category = getGroupValue(match, {"category", "type", "class"});
			if (!category.empty()) {
				event.category = category;
			} else {
				event.category = "regexp_match";
			}

			// Test name
			std::string test_name = getGroupValue(match, {"test_name", "test", "name"});
			if (!test_name.empty()) {
				event.test_name = test_name;
			}

			// Suggestion
			std::string suggestion = getGroupValue(match, {"suggestion", "fix", "hint"});
			if (!suggestion.empty()) {
				event.suggestion = suggestion;
			}

			// Tool name (can be overridden by pattern)
			std::string tool = getGroupValue(match, {"tool", "tool_name"});
			if (!tool.empty()) {
				event.tool_name = tool;
			}

			event.log_content = line;
			event.execution_time = 0.0;
			event.log_line_start = line_num;
			event.log_line_end = line_num;

			events.push_back(event);
		} else if (include_unparsed) {
			// Emit UNKNOWN event for unmatched lines when include_unparsed is true
			// This is primarily for debugging - only log_content is populated
			ValidationEvent unknown_event;
			unknown_event.event_id = event_id++;
			unknown_event.tool_name = "regexp";
			unknown_event.event_type = ValidationEventType::UNKNOWN;
			// status and severity deliberately left unset (will be NULL)
			unknown_event.category = "regexp_match";
			// message deliberately left empty - log_content is sufficient for debugging
			unknown_event.log_content = line;
			unknown_event.log_line_start = line_num;
			unknown_event.log_line_end = line_num;
			unknown_event.ref_line = line_num;
			unknown_event.ref_column = -1;
			unknown_event.execution_time = 0.0;

			events.push_back(unknown_event);
		}
	}

	// If no events were created, add a summary event
	if (events.empty()) {
		ValidationEvent summary_event;
		summary_event.event_id = 1;
		summary_event.tool_name = "regexp";
		summary_event.event_type = ValidationEventType::LINT_ISSUE;
		summary_event.status = ValidationEventStatus::INFO;
		summary_event.severity = "info";
		summary_event.category = "regexp_summary";
		summary_event.message = "No matches found for the provided pattern";
		summary_event.ref_line = -1;
		summary_event.ref_column = -1;
		summary_event.execution_time = 0.0;
		events.push_back(summary_event);
	}
}

} // namespace duckdb
