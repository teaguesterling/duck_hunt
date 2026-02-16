#include "black_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Black parsing (compiled once, reused)
namespace {
static const std::regex RE_BLACK_REFORMAT(R"(would reformat)");
static const std::regex RE_BLACK_SUMMARY(R"(\d+ files? would be reformatted)");
static const std::regex RE_BLACK_SUCCESS(R"(All done! ✨ 🍰 ✨)");
static const std::regex RE_WOULD_REFORMAT(R"(would reformat (.+))");
static const std::regex RE_REFORMAT_SUMMARY(R"((\d+) files? would be reformatted, (\d+) files? would be left unchanged)");
static const std::regex RE_ALL_DONE_SUMMARY(R"(All done! ✨ 🍰 ✨)");
static const std::regex RE_DIFF_HEADER(R"(--- (.+)\s+\(original\))");
} // anonymous namespace

bool BlackParser::canParse(const std::string &content) const {
	// Look for Black-specific patterns
	if (content.find("would reformat") != std::string::npos || content.find("All done! ✨ 🍰 ✨") != std::string::npos ||
	    content.find("files would be reformatted") != std::string::npos) {
		return isValidBlackOutput(content);
	}
	return false;
}

bool BlackParser::isValidBlackOutput(const std::string &content) const {
	// Check for Black-specific output patterns
	return std::regex_search(content, RE_BLACK_REFORMAT) || std::regex_search(content, RE_BLACK_SUMMARY) ||
	       std::regex_search(content, RE_BLACK_SUCCESS);
}

std::vector<ValidationEvent> BlackParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	bool in_diff_mode = false;
	std::string current_file;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Check for "would reformat" messages
		if (std::regex_search(line, match, RE_WOULD_REFORMAT)) {
			std::string file_path = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "File would be reformatted by Black";
			event.ref_file = file_path;
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "black";
			event.category = "code_formatting";
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"action\": \"would_reformat\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for reformat summary
		else if (std::regex_search(line, match, RE_REFORMAT_SUMMARY)) {
			std::string reformat_count = match[1].str();
			std::string unchanged_count = match[2].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
			event.message =
			    reformat_count + " files would be reformatted, " + unchanged_count + " files would be left unchanged";
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "black";
			event.category = "code_formatting";
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data =
			    "{\"reformat_count\": " + reformat_count + ", \"unchanged_count\": " + unchanged_count + "}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for "All done!" success message
		else if (std::regex_search(line, match, RE_ALL_DONE_SUMMARY)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.status = ValidationEventStatus::PASS;
			event.message = "Black formatting check completed successfully";
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "black";
			event.category = "code_formatting";
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"action\": \"success\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for diff header (unified diff mode)
		else if (std::regex_search(line, match, RE_DIFF_HEADER)) {
			current_file = match[1].str();
			in_diff_mode = true;

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Black would apply formatting changes";
			event.ref_file = current_file;
			event.ref_line = -1;
			event.ref_column = -1;
			event.tool_name = "black";
			event.category = "code_formatting";
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"action\": \"diff_start\", \"file\": \"" + current_file + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Handle diff content (lines starting with + or -)
		else if (in_diff_mode && (line.front() == '+' || line.front() == '-') && line.size() > 1) {
			// Skip pure markers like +++/---
			if (line.substr(0, 3) != "+++" && line.substr(0, 3) != "---") {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::LINT_ISSUE;
				event.severity = "info";
				event.status = ValidationEventStatus::INFO;

				if (line.front() == '+') {
					event.message = "Black would add: " + line.substr(1);
				} else {
					event.message = "Black would remove: " + line.substr(1);
				}

				event.ref_file = current_file;
				event.ref_line = -1;
				event.ref_column = -1;
				event.tool_name = "black";
				event.category = "code_formatting";
				event.execution_time = 0.0;
				event.log_content = line;
				event.structured_data =
				    "{\"action\": \"diff_line\", \"type\": \"" + std::string(1, line.front()) + "\"}";
				event.log_line_start = current_line_num;
				event.log_line_end = current_line_num;

				events.push_back(event);
			}
		}
		// Reset diff mode on empty lines or when encountering new files
		else if (line.empty() || line.find("would reformat") != std::string::npos) {
			in_diff_mode = false;
			current_file.clear();
		}
	}

	return events;
}

} // namespace duckdb
