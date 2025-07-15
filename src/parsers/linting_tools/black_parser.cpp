#include "black_parser.hpp"
#include "../../core/parser_registry.hpp"
#include <sstream>

namespace duckdb {

bool BlackParser::canParse(const std::string& content) const {
    // Look for Black-specific patterns
    if (content.find("would reformat") != std::string::npos ||
        content.find("All done! ✨ 🍰 ✨") != std::string::npos ||
        content.find("files would be reformatted") != std::string::npos) {
        return isValidBlackOutput(content);
    }
    return false;
}

bool BlackParser::isValidBlackOutput(const std::string& content) const {
    // Check for Black-specific output patterns
    std::regex black_reformat(R"(would reformat)");
    std::regex black_summary(R"(\d+ files? would be reformatted)");
    std::regex black_success(R"(All done! ✨ 🍰 ✨)");
    
    return std::regex_search(content, black_reformat) || 
           std::regex_search(content, black_summary) ||
           std::regex_search(content, black_success);
}

std::vector<ValidationEvent> BlackParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Black output
    std::regex would_reformat(R"(would reformat (.+))");
    std::regex reformat_summary(R"((\d+) files? would be reformatted, (\d+) files? would be left unchanged)");
    std::regex all_done_summary(R"(All done! ✨ 🍰 ✨)");
    std::regex diff_header(R"(--- (.+)\s+\(original\))");
    
    bool in_diff_mode = false;
    std::string current_file;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for "would reformat" messages
        if (std::regex_search(line, match, would_reformat)) {
            std::string file_path = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "File would be reformatted by Black";
            event.file_path = file_path;
            event.line_number = -1;
            event.column_number = -1;
            event.tool_name = "black";
            event.category = "code_formatting";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"action\": \"would_reformat\"}";
            
            events.push_back(event);
        }
        // Check for reformat summary
        else if (std::regex_search(line, match, reformat_summary)) {
            std::string reformat_count = match[1].str();
            std::string unchanged_count = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
            event.message = reformat_count + " files would be reformatted, " + unchanged_count + " files would be left unchanged";
            event.line_number = -1;
            event.column_number = -1;
            event.tool_name = "black";
            event.category = "code_formatting";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"reformat_count\": " + reformat_count + ", \"unchanged_count\": " + unchanged_count + "}";
            
            events.push_back(event);
        }
        // Check for "All done!" success message
        else if (std::regex_search(line, match, all_done_summary)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Black formatting check completed successfully";
            event.line_number = -1;
            event.column_number = -1;
            event.tool_name = "black";
            event.category = "code_formatting";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"action\": \"success\"}";
            
            events.push_back(event);
        }
        // Check for diff header (unified diff mode)
        else if (std::regex_search(line, match, diff_header)) {
            current_file = match[1].str();
            in_diff_mode = true;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
            event.message = "Black would apply formatting changes";
            event.file_path = current_file;
            event.line_number = -1;
            event.column_number = -1;
            event.tool_name = "black";
            event.category = "code_formatting";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"action\": \"diff_start\", \"file\": \"" + current_file + "\"}";
            
            events.push_back(event);
        }
        // Handle diff content (lines starting with + or -)
        else if (in_diff_mode && (line.front() == '+' || line.front() == '-') && line.size() > 1) {
            // Skip pure markers like +++/---
            if (line.substr(0, 3) != "+++" && line.substr(0, 3) != "---") {
                ValidationEvent event;
                event.event_id = event_id++;
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
                
                if (line.front() == '+') {
                    event.message = "Black would add: " + line.substr(1);
                } else {
                    event.message = "Black would remove: " + line.substr(1);
                }
                
                event.file_path = current_file;
                event.line_number = -1;
                event.column_number = -1;
                event.tool_name = "black";
                event.category = "code_formatting";
                event.execution_time = 0.0;
                event.raw_output = line;
                event.structured_data = "{\"action\": \"diff_line\", \"type\": \"" + std::string(1, line.front()) + "\"}";
                
                events.push_back(event);
            }
        }
        // Reset diff mode on empty lines or when encountering new files
        else if (line.empty() || line.find("would reformat") != std::string::npos) {
            in_diff_mode = false;
            current_file.clear();
        }
    }
    
    return events;
}

// Auto-register this parser
REGISTER_PARSER(BlackParser);

} // namespace duckdb