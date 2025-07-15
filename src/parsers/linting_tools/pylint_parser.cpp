#include "pylint_parser.hpp"
#include "../../core/parser_registry.hpp"
#include <sstream>

namespace duckdb {

bool PylintParser::canParse(const std::string& content) const {
    // Look for Pylint-specific patterns
    if (content.find("Module ") != std::string::npos ||
        content.find("Your code has been rated") != std::string::npos ||
        (content.find(": ") != std::string::npos && 
         (content.find(" C:") != std::string::npos || 
          content.find(" W:") != std::string::npos ||
          content.find(" E:") != std::string::npos ||
          content.find(" R:") != std::string::npos ||
          content.find(" F:") != std::string::npos))) {
        return isValidPylintOutput(content);
    }
    return false;
}

bool PylintParser::isValidPylintOutput(const std::string& content) const {
    // Check for Pylint-specific output patterns
    std::regex pylint_module(R"(\*+\s*Module\s+)");
    std::regex pylint_message(R"([CWERF]:\s*\d+,\s*\d+:)");
    std::regex pylint_rating(R"(Your code has been rated at)");
    
    return std::regex_search(content, pylint_module) || 
           std::regex_search(content, pylint_message) ||
           std::regex_search(content, pylint_rating);
}

std::vector<ValidationEvent> PylintParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Pylint output
    std::regex pylint_module_header(R"(\*+\s*Module\s+(.+))");
    std::regex pylint_message(R"(([CWERF]):\s*(\d+),\s*(\d+):\s*(.+?)\s+\(([^)]+)\))");  // C:  1, 0: message (code)
    std::regex pylint_message_simple(R"(([CWERF]):\s*(\d+),\s*(\d+):\s*(.+))");  // C:  1, 0: message
    std::regex pylint_rating(R"(Your code has been rated at ([\d\.-]+)/10)");
    std::regex pylint_statistics(R"((\d+)\s+statements\s+analysed)");
    
    std::string current_module;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for module header
        if (std::regex_search(line, match, pylint_module_header)) {
            current_module = match[1].str();
            continue;
        }
        
        // Check for Pylint message with error code
        if (std::regex_search(line, match, pylint_message)) {
            std::string severity_char = match[1].str();
            std::string line_str = match[2].str();
            std::string column_str = match[3].str();
            std::string message = match[4].str();
            std::string error_code = match[5].str();
            
            int64_t line_number = 0;
            int64_t column_number = 0;
            
            try {
                line_number = std::stoi(line_str);
                column_number = std::stoi(column_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            
            // Map Pylint severity to ValidationEventStatus
            if (severity_char == "E" || severity_char == "F") {
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
                event.event_type = ValidationEventType::BUILD_ERROR;
            } else if (severity_char == "W") {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity_char == "C" || severity_char == "R") {
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = current_module.empty() ? "unknown" : current_module;
            event.line_number = line_number;
            event.column_number = column_number;
            event.error_code = error_code;
            event.tool_name = "pylint";
            event.category = "code_quality";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"severity_char\": \"" + severity_char + "\", \"error_code\": \"" + error_code + "\"}";
            
            events.push_back(event);
        }
        // Check for Pylint message without explicit error code
        else if (std::regex_search(line, match, pylint_message_simple)) {
            std::string severity_char = match[1].str();
            std::string line_str = match[2].str();
            std::string column_str = match[3].str();
            std::string message = match[4].str();
            
            int64_t line_number = 0;
            int64_t column_number = 0;
            
            try {
                line_number = std::stoi(line_str);
                column_number = std::stoi(column_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            
            // Map Pylint severity to ValidationEventStatus
            if (severity_char == "E" || severity_char == "F") {
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
                event.event_type = ValidationEventType::BUILD_ERROR;
            } else if (severity_char == "W") {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity_char == "C" || severity_char == "R") {
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = current_module.empty() ? "unknown" : current_module;
            event.line_number = line_number;
            event.column_number = column_number;
            event.tool_name = "pylint";
            event.category = "code_quality";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"severity_char\": \"" + severity_char + "\"}";
            
            events.push_back(event);
        }
        // Check for rating information
        else if (std::regex_search(line, match, pylint_rating)) {
            std::string rating = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Code quality rating: " + rating + "/10";
            event.line_number = -1;
            event.column_number = -1;
            event.tool_name = "pylint";
            event.category = "code_quality";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"rating\": \"" + rating + "\"}";
            
            events.push_back(event);
        }
    }
    
    return events;
}

// Auto-register this parser
REGISTER_PARSER(PylintParser);

} // namespace duckdb