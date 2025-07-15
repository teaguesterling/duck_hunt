#include "cmake_parser.hpp"
#include "../../core/parser_registry.hpp"
#include <sstream>

namespace duckdb {

bool CMakeParser::canParse(const std::string& content) const {
    // Look for CMake-specific patterns
    if (content.find("CMake Error") != std::string::npos ||
        content.find("CMake Warning") != std::string::npos ||
        content.find("gmake[") != std::string::npos ||
        content.find("Configuring incomplete") != std::string::npos ||
        (content.find("undefined reference") != std::string::npos && 
         content.find("cmake") != std::string::npos)) {
        return isValidCMakeOutput(content);
    }
    return false;
}

bool CMakeParser::isValidCMakeOutput(const std::string& content) const {
    // Check for CMake-specific output patterns
    std::regex cmake_error(R"(CMake Error)");
    std::regex cmake_warning(R"(CMake Warning)");
    std::regex gmake_pattern(R"(gmake\[.*\])");
    std::regex config_incomplete(R"(Configuring incomplete)");
    
    return std::regex_search(content, cmake_error) || 
           std::regex_search(content, cmake_warning) ||
           std::regex_search(content, gmake_pattern) ||
           std::regex_search(content, config_incomplete);
}

std::vector<ValidationEvent> CMakeParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    while (std::getline(stream, line)) {
        // Parse GCC/Clang error format: file:line:column: severity: message
        std::regex cpp_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, cpp_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
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
                event.status = ValidationEventStatus::ERROR;  // Treat notes as errors for CMake builds
                event.category = "compilation";
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }
            
            event.raw_output = content;
            event.structured_data = "cmake_build";
            
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
        // Parse CMake warnings
        else if (line.find("CMake Warning") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
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
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
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
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
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
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
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
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "cmake";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
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
    
    return events;
}

// Auto-register this parser
REGISTER_PARSER(CMakeParser);

} // namespace duckdb