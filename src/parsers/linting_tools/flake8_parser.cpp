#include "flake8_parser.hpp"
#include <sstream>

namespace duckdb {

bool Flake8Parser::canParse(const std::string& content) const {
    // Look for flake8-specific patterns: error codes like F401, E302, W503, C901
    if (content.find(": F") != std::string::npos ||
        content.find(": E") != std::string::npos ||
        content.find(": W") != std::string::npos ||
        content.find(": C") != std::string::npos) {
        return isValidFlake8Output(content);
    }
    return false;
}

bool Flake8Parser::isValidFlake8Output(const std::string& content) const {
    // Check for flake8 output pattern: file.py:line:column: CODE message
    std::regex flake8_pattern(R"([^:]+:\d+:\d+:\s*[FEWC]\d+)");
    return std::regex_search(content, flake8_pattern);
}

std::vector<ValidationEvent> Flake8Parser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Regex pattern for Flake8 output: file.py:line:column: error_code message
    std::regex flake8_message(R"(([^:]+):(\d+):(\d+):\s*([FEWC]\d+)\s*(.+))");

    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;
        
        if (std::regex_search(line, match, flake8_message)) {
            std::string file_path = match[1].str();
            std::string line_str = match[2].str();
            std::string column_str = match[3].str();
            std::string error_code = match[4].str();
            std::string message = match[5].str();
            
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
            
            // Map Flake8 error codes to severity
            if (error_code.front() == 'F') {
                // F codes are pyflakes errors (logical errors)
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
                event.event_type = ValidationEventType::BUILD_ERROR;
            } else if (error_code.front() == 'E') {
                // E codes are PEP 8 errors (style errors)
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
            } else if (error_code.front() == 'W') {
                // W codes are PEP 8 warnings
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (error_code.front() == 'C') {
                // C codes are complexity warnings
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = file_path;
            event.line_number = line_number;
            event.column_number = column_number;
            event.error_code = error_code;
            event.tool_name = "flake8";
            event.category = "style_guide";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"error_code\": \"" + error_code + "\", \"error_type\": \"" + std::string(1, error_code.front()) + "\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
    }
    
    return events;
}

} // namespace duckdb