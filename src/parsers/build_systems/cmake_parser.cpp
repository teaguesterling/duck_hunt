#include "cmake_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

bool CMakeParser::canParse(const std::string &content) const {
	// Only match CMake-specific patterns.
	// GCC-style diagnostics (file:line: error:) should be handled by gcc_text parser.
	// CMake-specific patterns:
	// - "CMake Error" / "CMake Warning" (configuration messages)
	// - "-- Configuring incomplete" (configuration summary)
	// - "gmake[" with errors (cmake-invoked make)
	if (content.find("CMake Error") != std::string::npos) {
		return true;
	}
	if (content.find("CMake") != std::string::npos && content.find("Warning") != std::string::npos) {
		return true;
	}
	if (content.find("-- Configuring incomplete") != std::string::npos) {
		return true;
	}
	if (content.find("gmake[") != std::string::npos && content.find("Error") != std::string::npos) {
		return true;
	}
	return false;
}

std::vector<ValidationEvent> CMakeParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int64_t event_id = 1;

	// Pre-compiled regex patterns for CMake-specific patterns (these are safe - short lines expected)
	static const std::regex cmake_error_pattern(R"(CMake Error at ([^:]+):(\d+))");
	static const std::regex cmake_warning_pattern(R"(CMake Warning at ([^:]+):(\d+))");
	static const std::regex target_pattern(R"(\[([^:\]]+):(\d+):\s*([^\]]+)\])");

	while (reader.getLine(line)) {
		std::smatch match;
		int32_t current_line = reader.lineNumber();

		// Parse CMake configuration errors: "CMake Error at file:line ..."
		if (line.find("CMake Error") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "cmake";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.status = ValidationEventStatus::ERROR;
			event.category = "configuration";
			event.severity = "error";
			event.ref_line = -1;
			event.ref_column = -1;

			std::smatch cmake_match;
			if (SafeParsing::SafeRegexSearch(line, cmake_match, cmake_error_pattern)) {
				event.ref_file = cmake_match[1].str();
				event.ref_line = std::stoi(cmake_match[2].str());
			}

			event.message = line;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "cmake_build";
			event.log_line_start = current_line;
			event.log_line_end = current_line;

			events.push_back(event);
		}
		// Parse CMake warnings (including "CMake Warning", "CMake Deprecation Warning", "CMake Developer Warning")
		else if (line.find("CMake") != std::string::npos && line.find("Warning") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "cmake";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.status = ValidationEventStatus::WARNING;
			event.category = "configuration";
			event.severity = "warning";
			event.ref_line = -1;
			event.ref_column = -1;

			std::smatch cmake_match;
			if (SafeParsing::SafeRegexSearch(line, cmake_match, cmake_warning_pattern)) {
				event.ref_file = cmake_match[1].str();
				event.ref_line = std::stoi(cmake_match[2].str());
			}

			event.message = line;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "cmake_build";
			event.log_line_start = current_line;
			event.log_line_end = current_line;

			events.push_back(event);
		}
		// Parse gmake errors (cmake invokes gmake on some systems)
		else if (line.find("gmake[") != std::string::npos && line.find("***") != std::string::npos &&
		         line.find("Error") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "cmake";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.status = ValidationEventStatus::ERROR;
			event.category = "build_failure";
			event.severity = "error";
			event.message = line;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "cmake_build";
			event.log_line_start = current_line;
			event.log_line_end = current_line;

			// Extract target from pattern like "[target]"
			std::smatch target_match;
			if (SafeParsing::SafeRegexSearch(line, target_match, target_pattern)) {
				event.ref_file = target_match[1].str();
				event.test_name = target_match[3].str();
			}

			events.push_back(event);
		}
		// Parse CMake configuration summary errors
		else if (line.find("-- Configuring incomplete, errors occurred!") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "cmake";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.status = ValidationEventStatus::ERROR;
			event.category = "configuration";
			event.severity = "error";
			event.message = line;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "cmake_build";
			event.log_line_start = current_line;
			event.log_line_end = current_line;

			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
