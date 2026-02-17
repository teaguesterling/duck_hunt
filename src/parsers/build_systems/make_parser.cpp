#include "make_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

bool MakeParser::canParse(const std::string &content) const {
	// Only match when there are actual make-specific markers.
	// GCC-style diagnostics (file:line: error:) should be handled by gcc_text parser.
	// Make-specific patterns:
	// - "make: ***" (make error message)
	// - "make[N]:" (submake output)
	// - "Entering directory" / "Leaving directory"
	if (content.find("make: ***") != std::string::npos) {
		return true;
	}
	if (content.find("make[") != std::string::npos) {
		// Verify it's actually make output (has directory or error info)
		return content.find("Entering directory") != std::string::npos ||
		       content.find("Leaving directory") != std::string::npos || content.find("Error") != std::string::npos;
	}
	return false;
}

std::vector<ValidationEvent> MakeParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	SafeParsing::SafeLineReader reader(content);
	std::string line;
	int64_t event_id = 1;

	// Pre-compiled regex patterns for short, predictable patterns only
	static const std::regex target_pattern(R"(\[([^:\]]+):(\d+):\s*([^\]]+)\])");

	while (reader.getLine(line)) {
		int32_t current_line_num = reader.lineNumber();

		// Parse make failure line with target extraction: "make: *** [target] Error N"
		if (line.find("make: ***") != std::string::npos && line.find("Error") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "make";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.status = ValidationEventStatus::ERROR;
			event.category = "build_failure";
			event.severity = "error";
			event.message = line;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "make_build";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			// Extract makefile target from pattern like "[Makefile:23: build/main]"
			// This regex is safe - the pattern has bounded character classes [^:\]]+
			std::smatch target_match;
			if (SafeParsing::SafeRegexSearch(line, target_match, target_pattern)) {
				event.ref_file = target_match[1].str(); // Makefile
				// Don't extract line_number for make build failures - keep it as -1 (NULL)
				event.test_name = target_match[3].str(); // Target name (e.g., "build/main")
			}

			events.push_back(event);
		}
		// Parse make[N]: entering/leaving directory (info for context)
		else if (line.find("make[") != std::string::npos && (line.find("Entering directory") != std::string::npos ||
		                                                     line.find("Leaving directory") != std::string::npos)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "make";
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.status = ValidationEventStatus::INFO;
			event.category = "build_context";
			event.severity = "info";
			event.message = line;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "make_build";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			// Extract directory path
			size_t dir_start = line.find("directory '");
			if (dir_start != std::string::npos) {
				dir_start += 11; // length of "directory '"
				size_t dir_end = line.find("'", dir_start);
				if (dir_end != std::string::npos) {
					event.ref_file = line.substr(dir_start, dir_end - dir_start);
				}
			}

			events.push_back(event);
		}
		// Parse make[N]: error lines (submake failures)
		else if (line.find("make[") != std::string::npos && line.find("***") != std::string::npos &&
		         line.find("Error") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "make";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.status = ValidationEventStatus::ERROR;
			event.category = "build_failure";
			event.severity = "error";
			event.message = line;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "make_build";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			// Extract target from pattern like "[target]"
			std::smatch target_match;
			if (SafeParsing::SafeRegexSearch(line, target_match, target_pattern)) {
				event.ref_file = target_match[1].str();
				event.test_name = target_match[3].str();
			}

			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
