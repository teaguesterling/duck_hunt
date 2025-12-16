#include "make_parser.hpp"
#include <sstream>

namespace duckdb {

bool MakeParser::canParse(const std::string& content) const {
    // Look for make-specific patterns
    if (content.find("make: ***") != std::string::npos ||
        content.find("undefined reference") != std::string::npos ||
        content.find(": error:") != std::string::npos ||
        content.find(": warning:") != std::string::npos) {
        return isValidMakeError(content);
    }
    return false;
}

bool MakeParser::isValidMakeError(const std::string& content) const {
    // Check for GCC/Clang error patterns or make failure patterns
    std::regex gcc_pattern(R"([^:]+:\d+:\d*:?\s*(error|warning|note):)");
    std::regex make_pattern(R"(make: \*\*\*.*Error)");
    
    return std::regex_search(content, gcc_pattern) || 
           std::regex_search(content, make_pattern);
}

std::vector<ValidationEvent> MakeParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_function;
    int32_t current_line_num = 0;  // Track position in log file (1-indexed)

    while (std::getline(stream, line)) {
        current_line_num++;  // Increment before processing (1-indexed)
        // Parse function context: "file.c: In function 'function_name':"
        std::regex function_pattern(R"(([^:]+):\s*In function\s+'([^']+)':)");
        std::smatch func_match;
        if (std::regex_match(line, func_match, function_pattern)) {
            current_function = func_match[2].str();
            continue;
        }
        
        // Parse GCC/Clang error format: file:line:column: severity: message
        std::regex error_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|note):\s*(.+))");
        std::smatch match;
        
        if (std::regex_match(line, match, error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = current_function;
            event.message = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            
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
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "compilation";
                event.severity = "info";
            }
            
            events.push_back(event);
        }
        // Parse make failure line with target extraction
        else if (line.find("make: ***") != std::string::npos && line.find("Error") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "build_failure";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            
            // Extract makefile target from pattern like "[Makefile:23: build/main]"
            // Note: We extract file_path and test_name but NOT line_number for make build failures
            std::regex target_pattern(R"(\[([^:]+):(\d+):\s*([^\]]+)\])");
            std::smatch target_match;
            if (std::regex_search(line, target_match, target_pattern)) {
                event.file_path = target_match[1].str();  // Makefile
                // Don't extract line_number for make build failures - keep it as -1 (NULL)
                event.test_name = target_match[3].str();  // Target name (e.g., "build/main")
            }
            
            events.push_back(event);
        }
        // Parse linker errors for make builds
        else if (line.find("undefined reference") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "make";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "linking";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "make_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            
            // Extract symbol name from undefined reference
            std::regex symbol_pattern(R"(undefined reference to `([^']+)')");
            std::smatch symbol_match;
            if (std::regex_search(line, symbol_match, symbol_pattern)) {
                event.function_name = symbol_match[1].str();
                event.suggestion = "Link the library containing '" + event.function_name + "'";
            }
            
            events.push_back(event);
        }
    }
    
    return events;
}


} // namespace duckdb