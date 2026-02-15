#include "yapf_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for YAPF text parsing (compiled once, reused)
namespace {
static const std::regex RE_DIFF_START_YAPF(R"(--- a/(.+) \(original\))");
static const std::regex RE_DIFF_FIXED_YAPF(R"(\+\+\+ b/(.+) \(reformatted\))");
static const std::regex RE_REFORMATTED_FILE(R"(Reformatted (.+))");
static const std::regex RE_YAPF_COMMAND(R"(yapf (--[^\s]+.+))");
static const std::regex RE_PROCESSING_VERBOSE(R"(Processing (.+))");
static const std::regex RE_STYLE_CONFIG(R"(Style configuration: (.+))");
static const std::regex RE_LINE_LENGTH_CONFIG(R"(Line length: (\d+))");
static const std::regex RE_INDENT_WIDTH_CONFIG(R"(Indent width: (\d+))");
static const std::regex RE_FILES_PROCESSED(R"(Files processed: (\d+))");
static const std::regex RE_FILES_REFORMATTED(R"(Files reformatted: (\d+))");
static const std::regex RE_FILES_NO_CHANGES(R"(Files with no changes: (\d+))");
static const std::regex RE_EXECUTION_TIME(R"(Total execution time: ([\d\.]+)s)");
static const std::regex RE_CHECK_ERROR(R"(ERROR: Files would be reformatted but yapf was run with --check)");
static const std::regex RE_YAPF_ERROR(R"(yapf: error: (.+))");
static const std::regex RE_SYNTAX_ERROR(R"(ERROR: ([^:]+\.py):(\d+):(\d+): (.+))");
static const std::regex RE_ENCODING_WARNING(R"(WARNING: ([^:]+\.py): cannot determine encoding)");
static const std::regex RE_INFO_NO_CHANGES(R"(INFO: ([^:]+\.py): no changes needed)");
static const std::regex RE_FILES_LEFT_UNCHANGED(R"((\d+) files reformatted, (\d+) files left unchanged\.)");
} // anonymous namespace

bool YapfTextParser::canParse(const std::string &content) const {
	// First check for exclusions - if this is Ansible output, don't parse as YAPF
	if (content.find("PLAY [") != std::string::npos || content.find("TASK [") != std::string::npos ||
	    content.find("PLAY RECAP") != std::string::npos) {
		return false;
	}

	// Check for YAPF-specific patterns
	return content.find("yapf") != std::string::npos || content.find("Reformatted ") != std::string::npos ||
	       (content.find("--- a/") != std::string::npos && content.find("(original)") != std::string::npos) ||
	       (content.find("+++ b/") != std::string::npos && content.find("(reformatted)") != std::string::npos) ||
	       content.find("files reformatted") != std::string::npos ||
	       content.find("Files processed:") != std::string::npos;
}

std::vector<ValidationEvent> YapfTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::smatch match;
	std::string current_file;

	while (std::getline(stream, line)) {
		current_line_num++;
		// Handle yapf diff sections
		if (std::regex_search(line, match, RE_DIFF_START_YAPF)) {
			current_file = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = current_file;
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "formatting";
			event.message = "File formatting changes detected";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle reformatted file patterns
		if (std::regex_search(line, match, RE_REFORMATTED_FILE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "formatting";
			event.message = "File reformatted";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle yapf command patterns
		if (std::regex_search(line, match, RE_YAPF_COMMAND)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "configuration";
			event.message = "Command: yapf " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle verbose processing
		if (std::regex_search(line, match, RE_PROCESSING_VERBOSE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "processing";
			event.message = "Processing file";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle style configuration
		if (std::regex_search(line, match, RE_STYLE_CONFIG)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "configuration";
			event.message = "Style configuration: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle line length configuration
		if (std::regex_search(line, match, RE_LINE_LENGTH_CONFIG)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "configuration";
			event.message = "Line length: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle indent width configuration
		if (std::regex_search(line, match, RE_INDENT_WIDTH_CONFIG)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "configuration";
			event.message = "Indent width: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle summary statistics
		if (std::regex_search(line, match, RE_FILES_PROCESSED)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "summary";
			event.message = "Files processed: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_FILES_REFORMATTED)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "summary";
			event.message = "Files reformatted: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_FILES_NO_CHANGES)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "summary";
			event.message = "Files with no changes: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_EXECUTION_TIME)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "performance";
			event.message = "Execution time: " + match[1].str() + "s";
			event.execution_time = SafeParsing::SafeStod(match[1].str());
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle combined summary (e.g., "5 files reformatted, 3 files left unchanged.")
		if (std::regex_search(line, match, RE_FILES_LEFT_UNCHANGED)) {
			ValidationEvent event1;
			event1.event_id = event_id++;
			event1.tool_name = "yapf";
			event1.event_type = ValidationEventType::SUMMARY;
			event1.ref_file = "";
			event1.ref_line = -1;
			event1.ref_column = -1;
			event1.status = ValidationEventStatus::INFO;
			event1.severity = "info";
			event1.category = "summary";
			event1.message = "Files reformatted: " + match[1].str();
			event1.execution_time = 0.0;
			event1.log_content = content;
			event1.structured_data = "yapf_text";

			ValidationEvent event2;
			event2.event_id = event_id++;
			event2.tool_name = "yapf";
			event2.event_type = ValidationEventType::SUMMARY;
			event2.ref_file = "";
			event2.ref_line = -1;
			event2.ref_column = -1;
			event2.status = ValidationEventStatus::INFO;
			event2.severity = "info";
			event2.category = "summary";
			event2.message = "Files left unchanged: " + match[2].str();
			event2.execution_time = 0.0;
			event2.log_content = content;
			event2.structured_data = "yapf_text";
			event1.log_line_start = current_line_num;
			event1.log_line_end = current_line_num;
			event2.log_line_start = current_line_num;
			event2.log_line_end = current_line_num;

			events.push_back(event1);
			events.push_back(event2);
			continue;
		}

		// Handle check mode error
		if (std::regex_search(line, match, RE_CHECK_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "check_mode";
			event.message = "Files would be reformatted but yapf was run with --check";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle yapf errors
		if (std::regex_search(line, match, RE_YAPF_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "command_error";
			event.message = match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle syntax errors
		if (std::regex_search(line, match, RE_SYNTAX_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = match[1].str();
			event.ref_line = SafeParsing::SafeStoi(match[2].str());
			event.ref_column = SafeParsing::SafeStoi(match[3].str());
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "syntax";
			event.message = match[4].str();
			event.error_code = "SyntaxError";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle encoding warnings
		if (std::regex_search(line, match, RE_ENCODING_WARNING)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "encoding";
			event.message = "Cannot determine encoding";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Handle info messages (no changes needed)
		if (std::regex_search(line, match, RE_INFO_NO_CHANGES)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "yapf";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "formatting";
			event.message = "No changes needed";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "yapf_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}
	}

	return events;
}

} // namespace duckdb
