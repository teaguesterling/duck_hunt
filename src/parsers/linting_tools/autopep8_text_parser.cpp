#include "autopep8_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for autopep8 text parsing (compiled once, reused)
namespace {
static const std::regex RE_DIFF_START(R"(--- original/(.+))");
static const std::regex RE_DIFF_FIXED(R"(\+\+\+ fixed/(.+))");
static const std::regex RE_ERROR_PATTERN(R"(ERROR: ([^:]+\.py):(\d+):(\d+): (E\d+) (.+))");
static const std::regex RE_WARNING_PATTERN(R"(WARNING: ([^:]+\.py):(\d+):(\d+): (E\d+) (.+))");
static const std::regex RE_INFO_PATTERN(R"(INFO: ([^:]+\.py): (.+))");
static const std::regex RE_FIXED_PATTERN(R"(fixed ([^:]+\.py))");
static const std::regex RE_AUTOPEP8_CMD(R"(autopep8 (--[^\s]+.+))");
static const std::regex RE_CONFIG_LINE(R"(Applied configuration:)");
static const std::regex RE_SUMMARY_FILES_PROCESSED(R"(Files processed: (\d+))");
static const std::regex RE_SUMMARY_FILES_MODIFIED(R"(Files modified: (\d+))");
static const std::regex RE_SUMMARY_FILES_ERRORS(R"(Files with errors: (\d+))");
static const std::regex RE_SUMMARY_FIXES_APPLIED(R"(Total fixes applied: (\d+))");
static const std::regex RE_SUMMARY_EXECUTION_TIME(R"(Execution time: ([\d\.]+)s)");
static const std::regex RE_SYNTAX_ERROR(R"(ERROR: ([^:]+\.py):(\d+):(\d+): SyntaxError: (.+))");
static const std::regex RE_ENCODING_ERROR(R"(WARNING: ([^:]+\.py): could not determine file encoding)");
static const std::regex RE_ALREADY_FORMATTED(R"(INFO: ([^:]+\.py): already formatted correctly)");
} // anonymous namespace

bool Autopep8TextParser::canParse(const std::string &content) const {
	// Check for autopep8-specific patterns
	if ((content.find("autopep8 --") != std::string::npos && content.find("--") != std::string::npos) ||
	    (content.find("--- original/") != std::string::npos && content.find("+++ fixed/") != std::string::npos) ||
	    (content.find("Files processed:") != std::string::npos &&
	     content.find("Files modified:") != std::string::npos)) {
		return true;
	}
	return false;
}

std::vector<ValidationEvent> Autopep8TextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;

	std::smatch match;
	std::string current_file;
	bool in_config = false;

	while (std::getline(stream, line)) {
		// Handle diff sections
		if (std::regex_search(line, match, RE_DIFF_START)) {
			current_file = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
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
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle error patterns (E999 syntax errors)
		if (std::regex_search(line, match, RE_ERROR_PATTERN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = match[1].str();
			event.ref_line = SafeParsing::SafeStoi(match[2].str());
			event.ref_column = SafeParsing::SafeStoi(match[3].str());
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "syntax";
			event.message = match[5].str();
			event.error_code = match[4].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle warning patterns (line too long)
		if (std::regex_search(line, match, RE_WARNING_PATTERN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = match[1].str();
			event.ref_line = SafeParsing::SafeStoi(match[2].str());
			event.ref_column = SafeParsing::SafeStoi(match[3].str());
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "style";
			event.message = match[5].str();
			event.error_code = match[4].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle info patterns (no changes needed)
		if (std::regex_search(line, match, RE_INFO_PATTERN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "formatting";
			event.message = match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle fixed file patterns
		if (std::regex_search(line, match, RE_FIXED_PATTERN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "formatting";
			event.message = "File formatting applied";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle command patterns
		if (std::regex_search(line, match, RE_AUTOPEP8_CMD)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "configuration";
			event.message = "Command: autopep8 " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle configuration section
		if (std::regex_search(line, match, RE_CONFIG_LINE)) {
			in_config = true;

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "configuration";
			event.message = "Configuration applied";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle summary statistics
		if (std::regex_search(line, match, RE_SUMMARY_FILES_PROCESSED)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
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
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_SUMMARY_FILES_MODIFIED)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "summary";
			event.message = "Files modified: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_SUMMARY_FILES_ERRORS)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "summary";
			event.message = "Files with errors: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_SUMMARY_FIXES_APPLIED)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "summary";
			event.message = "Total fixes applied: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		if (std::regex_search(line, match, RE_SUMMARY_EXECUTION_TIME)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "performance";
			event.message = "Execution time: " + match[1].str() + "s";
			event.execution_time = std::stod(match[1].str());
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle syntax errors
		if (std::regex_search(line, match, RE_SYNTAX_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
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
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle encoding errors
		if (std::regex_search(line, match, RE_ENCODING_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "encoding";
			event.message = "Could not determine file encoding";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Handle already formatted files
		if (std::regex_search(line, match, RE_ALREADY_FORMATTED)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = match[1].str();
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "formatting";
			event.message = "Already formatted correctly";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// Configuration line continuation
		if (in_config && line.find(":") != std::string::npos && line.find(" ") == 0) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "autopep8";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "configuration";
			event.message = line.substr(2); // Remove leading spaces
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "autopep8_text";

			events.push_back(event);
			continue;
		}

		// End configuration section when we hit an empty line
		if (in_config && line.empty()) {
			in_config = false;
		}
	}

	return events;
}

} // namespace duckdb
