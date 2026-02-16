#include "unity_editor_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns (compiled once, reused)
namespace {
// C# compiler error/warning: file.cs(line,col): error CS0234: message
static const std::regex RE_CS_ERROR(R"((.+?\.cs)\((\d+),(\d+)\):\s*(error|warning)\s+(CS\d+):\s*(.+))");

// Unity build progress: [545/613 0s] Action description
static const std::regex RE_BUILD_PROGRESS(R"(\[(\d+)/(\d+)\s+[\d\.]+s?\]\s*(.+))");

// Unity module message: [ModuleName] message or [ModuleName::SubModule] message
static const std::regex RE_MODULE_MESSAGE(R"(\[([A-Za-z_:]+)\]\s*(.+))");

// Script compilation error header
static const std::regex RE_SCRIPT_ERROR_HEADER(R"(Script Compilation Error for:\s*(.+))");

// Build result
static const std::regex RE_BUILD_RESULT(R"(Build\s+(succeeded|FAILED|completed))");

// DisplayProgressbar messages
static const std::regex RE_PROGRESS_BAR(R"(DisplayProgressbar:\s*(.+))");

// Unity version header
static const std::regex RE_UNITY_VERSION(R"(Unity Editor version:\s*(\S+))");

// Time elapsed
static const std::regex RE_TIME_ELAPSED(R"(Time Elapsed[:\s]+(\d+):(\d+):(\d+)\.?(\d*))");
} // anonymous namespace

bool UnityEditorParser::canParse(const std::string &content) const {
	// Check for Unity-specific markers
	if (content.find("Unity Editor version:") != std::string::npos) {
		return true;
	}
	if (content.find("unity-editor") != std::string::npos) {
		return true;
	}
	if (content.find("[Licensing::") != std::string::npos) {
		return true;
	}
	if (content.find("DisplayProgressbar:") != std::string::npos) {
		return true;
	}
	// Check for C# errors with .cs files but without [project.csproj] suffix
	// This distinguishes from MSBuild which has project suffix
	if (content.find(".cs(") != std::string::npos &&
	    (content.find("): error CS") != std::string::npos || content.find("): warning CS") != std::string::npos) &&
	    content.find(".csproj]") == std::string::npos) {
		return true;
	}
	return false;
}

std::vector<ValidationEvent> UnityEditorParser::parseLine(const std::string &line, int32_t line_number,
                                                          int64_t &event_id) const {
	std::vector<ValidationEvent> events;
	std::smatch match;

	// C# compiler error/warning
	if (std::regex_search(line, match, RE_CS_ERROR)) {
		ValidationEvent event;
		event.event_id = event_id++;
		event.tool_name = "unity-csc";
		event.event_type = ValidationEventType::BUILD_ERROR;
		event.ref_file = match[1].str();
		event.ref_line = SafeParsing::SafeStoi(match[2].str());
		event.ref_column = SafeParsing::SafeStoi(match[3].str());
		event.error_code = match[5].str();
		event.message = match[6].str();
		event.log_line_start = line_number;
		event.log_line_end = line_number;
		event.log_content = line;
		event.category = "compilation";

		std::string severity_str = match[4].str();
		if (severity_str == "error") {
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
		} else {
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
		}
		events.push_back(event);
		return events;
	}

	// Module error messages (e.g., [Licensing::Module] Error: ...)
	if (std::regex_search(line, match, RE_MODULE_MESSAGE)) {
		std::string module = match[1].str();
		std::string message = match[2].str();

		// Only emit events for error/warning messages
		if (message.find("Error:") != std::string::npos || message.find("error:") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "unity";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.category = module;
			event.message = message;
			event.severity = "error";
			event.status = ValidationEventStatus::ERROR;
			event.log_line_start = line_number;
			event.log_line_end = line_number;
			event.log_content = line;
			events.push_back(event);
		} else if (message.find("Warning:") != std::string::npos || message.find("warning:") != std::string::npos) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "unity";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.category = module;
			event.message = message;
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
			event.log_line_start = line_number;
			event.log_line_end = line_number;
			event.log_content = line;
			events.push_back(event);
		}
		return events;
	}

	// Build result
	if (std::regex_search(line, match, RE_BUILD_RESULT)) {
		ValidationEvent event;
		event.event_id = event_id++;
		event.tool_name = "unity";
		event.event_type = ValidationEventType::SUMMARY;
		event.category = "build_result";
		event.log_line_start = line_number;
		event.log_line_end = line_number;
		event.log_content = line;

		std::string result = match[1].str();
		if (result == "succeeded" || result == "completed") {
			event.message = "Build " + result;
			event.severity = "info";
			event.status = ValidationEventStatus::PASS;
		} else {
			event.message = "Build FAILED";
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
		}
		events.push_back(event);
		return events;
	}

	// Script compilation error header
	if (std::regex_search(line, match, RE_SCRIPT_ERROR_HEADER)) {
		ValidationEvent event;
		event.event_id = event_id++;
		event.tool_name = "unity";
		event.event_type = ValidationEventType::BUILD_ERROR;
		event.category = "script_compilation";
		event.message = "Script Compilation Error: " + match[1].str();
		event.severity = "error";
		event.status = ValidationEventStatus::ERROR;
		event.log_line_start = line_number;
		event.log_line_end = line_number;
		event.log_content = line;
		events.push_back(event);
		return events;
	}

	return events;
}

std::vector<ValidationEvent> UnityEditorParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 200); // Estimate based on typical log density

	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	// Track Unity version for metadata
	std::string unity_version;

	while (std::getline(stream, line)) {
		line_num++;

		// Extract Unity version for context
		std::smatch match;
		if (unity_version.empty() && std::regex_search(line, match, RE_UNITY_VERSION)) {
			unity_version = match[1].str();
		}

		// Parse line and collect events
		auto line_events = parseLine(line, line_num, event_id);
		for (auto &event : line_events) {
			// Add Unity version to scope if available
			if (!unity_version.empty() && event.scope.empty()) {
				event.scope = "Unity " + unity_version;
			}
			events.push_back(std::move(event));
		}
	}

	return events;
}

} // namespace duckdb
