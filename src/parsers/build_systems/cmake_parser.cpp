#include "cmake_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

bool CMakeParser::canParse(const std::string& content) const {
    // Check for CMake patterns (including "CMake Deprecation Warning", "CMake Developer Warning", etc.)
    bool has_cmake_warning = (content.find("CMake") != std::string::npos && content.find("Warning") != std::string::npos);
    return (content.find("CMake Error") != std::string::npos) ||
           has_cmake_warning ||
           (content.find("undefined reference") != std::string::npos) ||
           (content.find("collect2: error:") != std::string::npos) ||
           (content.find("gmake[") != std::string::npos && content.find("Error") != std::string::npos) ||
           (content.find("-- Configuring incomplete, errors occurred!") != std::string::npos) ||
           (content.find(".cpp:") != std::string::npos && content.find("error:") != std::string::npos);
}

std::vector<ValidationEvent> CMakeParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int64_t event_id = 1;

    // Pre-compiled regex patterns for CMake-specific patterns (these are safe - short lines expected)
    static const std::regex cmake_error_pattern(R"(CMake Error at ([^:]+):(\d+))");
    static const std::regex cmake_warning_pattern(R"(CMake Warning at ([^:]+):(\d+))");
    static const std::regex linker_pattern(R"(undefined reference to `([^`]+)`)");

    while (reader.getLine(line)) {
        std::smatch match;
        int32_t current_line = reader.lineNumber();

        // Parse GCC/Clang error format using safe string parsing (no regex backtracking risk)
        std::string file, severity, message;
        int line_num, col;
        if (SafeParsing::ParseCompilerDiagnostic(line, file, line_num, col, severity, message)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.ref_file = file;
            event.ref_line = line_num;
            event.ref_column = col;
            event.function_name = "";
            event.message = message;
            event.execution_time = 0.0;

            if (severity == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "compilation";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "compilation";
                event.severity = "warning";
            } else if (severity == "note") {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }

            event.log_content = line;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line;
            event.log_line_end = current_line;

            events.push_back(event);
        }
        // Parse CMake configuration errors
        else if (line.find("CMake Error") != std::string::npos) {
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
        // Parse linker errors
        else if (line.find("undefined reference") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.ref_line = -1;
            event.ref_column = -1;

            std::smatch linker_match;
            if (SafeParsing::SafeRegexSearch(line, linker_match, linker_pattern)) {
                event.function_name = linker_match[1].str();
                event.suggestion = "Link the library containing '" + event.function_name + "'";
            }

            event.message = line;
            event.execution_time = 0.0;
            event.log_content = line;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line;
            event.log_line_end = current_line;

            events.push_back(event);
        }
        // Parse collect2 linker errors
        else if (line.find("collect2: error:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.ref_line = -1;
            event.ref_column = -1;
            event.message = line;
            event.execution_time = 0.0;
            event.log_content = line;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line;
            event.log_line_end = current_line;

            events.push_back(event);
        }
        // Parse gmake errors
        else if (line.find("gmake[") != std::string::npos && line.find("***") != std::string::npos && line.find("Error") != std::string::npos) {
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
