#include "cmake_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool CMakeParser::CanParse(const std::string& content) const {
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

void CMakeParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseCMakeBuild(content, events);
}

void CMakeParser::ParseCMakeBuild(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        // Parse GCC/Clang error format: file:line:column: severity: message
        std::regex cpp_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, cpp_error_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = "";
            event.message = match[5].str();
            event.execution_time = 0.0;
            
            std::string severity = match[4].str();
            if (severity == "error") {
                event.status = duckdb::ValidationEventStatus::ERROR;
                event.category = "compilation";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = duckdb::ValidationEventStatus::WARNING;
                event.category = "compilation";
                event.severity = "warning";
            } else if (severity == "note") {
                event.status = duckdb::ValidationEventStatus::ERROR;  // Treat notes as errors for CMake builds
                event.category = "compilation";
                event.severity = "error";
            } else {
                event.status = duckdb::ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }
            
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake configuration errors
        else if (line.find("CMake Error") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "configuration";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract file info from CMake errors like "CMake Error at CMakeLists.txt:25"
            std::regex cmake_error_pattern(R"(CMake Error at ([^:]+):(\d+))");
            std::smatch cmake_match;
            if (std::regex_search(line, cmake_match, cmake_error_pattern)) {
                event.file_path = cmake_match[1].str();
                event.line_number = std::stoi(cmake_match[2].str());
            }
            
            event.message = content;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake warnings (including "CMake Warning", "CMake Deprecation Warning", "CMake Developer Warning")
        else if (line.find("CMake") != std::string::npos && line.find("Warning") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::WARNING;
            event.category = "configuration";
            event.severity = "warning";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract file info from CMake warnings
            std::regex cmake_warning_pattern(R"(CMake Warning at ([^:]+):(\d+))");
            std::smatch cmake_match;
            if (std::regex_search(line, cmake_match, cmake_warning_pattern)) {
                event.file_path = cmake_match[1].str();
                event.line_number = std::stoi(cmake_match[2].str());
            }
            
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse linker errors (both with and without /usr/bin/ld: prefix)
        else if (line.find("undefined reference") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            
            // Extract symbol name from linker error
            std::regex linker_pattern(R"(undefined reference to `([^`]+)`)");
            std::smatch linker_match;
            if (std::regex_search(line, linker_match, linker_pattern)) {
                event.function_name = linker_match[1].str();
                event.suggestion = "Link the library containing '" + event.function_name + "'";
            }
            
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse collect2 linker errors
        else if (line.find("collect2: error:") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.line_number = -1;
            event.column_number = -1;
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse gmake errors
        else if (line.find("gmake[") != std::string::npos && line.find("***") != std::string::npos && line.find("Error") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "build_failure";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
        // Parse CMake configuration summary errors  
        else if (line.find("-- Configuring incomplete, errors occurred!") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "configuration";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
            events.push_back(event);
        }
    }
}

} // namespace duck_hunt