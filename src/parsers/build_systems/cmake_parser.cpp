#include "cmake_parser.hpp"
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
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Pre-compiled regex patterns for performance
    static const std::regex cpp_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|note):\s*(.+))");
    static const std::regex cmake_error_pattern(R"(CMake Error at ([^:]+):(\d+))");
    static const std::regex cmake_warning_pattern(R"(CMake Warning at ([^:]+):(\d+))");
    static const std::regex linker_pattern(R"(undefined reference to `([^`]+)`)");

    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;

        // Parse GCC/Clang error format: file:line:column: severity: message
        if (std::regex_match(line, match, cpp_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.ref_file = match[1].str();
            event.ref_line = std::stoi(match[2].str());
            event.ref_column = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = "";
            event.message = match[5].str();
            event.execution_time = 0.0;

            std::string severity = match[4].str();
            if (severity == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "compilation";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "compilation";
                event.severity = "warning";
            } else if (severity == "note") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "compilation";
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }

            event.log_content = content;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

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
            if (std::regex_search(line, cmake_match, cmake_error_pattern)) {
                event.ref_file = cmake_match[1].str();
                event.ref_line = std::stoi(cmake_match[2].str());
            }

            event.message = content;
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

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
            if (std::regex_search(line, cmake_match, cmake_warning_pattern)) {
                event.ref_file = cmake_match[1].str();
                event.ref_line = std::stoi(cmake_match[2].str());
            }

            event.message = line;
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

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
            if (std::regex_search(line, linker_match, linker_pattern)) {
                event.function_name = linker_match[1].str();
                event.suggestion = "Link the library containing '" + event.function_name + "'";
            }

            event.message = line;
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

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
            event.log_content = content;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

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
            event.log_content = content;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

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
            event.log_content = content;
            event.structured_data = "cmake_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
    }

    return events;
}

} // namespace duckdb
