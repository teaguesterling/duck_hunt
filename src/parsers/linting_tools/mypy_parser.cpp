#include "mypy_parser.hpp"
#include "../../core/parser_registry.hpp"
#include <sstream>

namespace duckdb {

bool MypyParser::canParse(const std::string& content) const {
    // First check for clang-tidy specific patterns and exclude them
    std::vector<std::string> clang_tidy_rules = {
        "readability-", "performance-", "modernize-", "bugprone-", 
        "cppcoreguidelines-", "google-", "llvm-", "misc-", "portability-"
    };
    
    for (const auto& rule : clang_tidy_rules) {
        if (content.find(rule) != std::string::npos) {
            return false;  // This is likely clang-tidy output
        }
    }
    
    // Check for column numbers which are more common in clang-tidy than mypy
    std::regex clang_tidy_pattern(R"([^:]+:\d+:\d+:\s*(error|warning|note):\s*)");
    if (std::regex_search(content, clang_tidy_pattern)) {
        return false;  // This looks like clang-tidy format
    }
    
    // Look for mypy-specific patterns
    if (content.find("error:") != std::string::npos ||
        content.find("warning:") != std::string::npos ||
        content.find("Success: no issues found") != std::string::npos ||
        (content.find("Found") != std::string::npos && content.find("errors") != std::string::npos)) {
        return isValidMypyOutput(content);
    }
    return false;
}

bool MypyParser::isValidMypyOutput(const std::string& content) const {
    // Check for mypy-specific output patterns
    std::regex mypy_pattern(R"([^:]+:\d+:\s*(error|warning|note):)");
    std::regex mypy_summary(R"(Found \d+ errors? in \d+ files?)");
    std::regex mypy_success(R"(Success: no issues found in \d+ source files?)");
    
    return std::regex_search(content, mypy_pattern) || 
           std::regex_search(content, mypy_summary) ||
           std::regex_search(content, mypy_success);
}

std::vector<ValidationEvent> MypyParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for mypy output
    std::regex mypy_message(R"(([^:]+):(\d+):\s*(error|warning|note):\s*(.+?)\s*\[([^\]]+)\])");
    std::regex mypy_message_no_code(R"(([^:]+):(\d+):\s*(error|warning|note):\s*(.+))");
    std::regex mypy_summary(R"(Found (\d+) errors? in (\d+) files? \(checked (\d+) files?\))");
    std::regex mypy_success(R"(Success: no issues found in (\d+) source files?)");
    std::regex mypy_revealed_type(R"((.+):(\d+):\s*note:\s*Revealed type is \"(.+)\")");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for mypy message with error code
        if (std::regex_search(line, match, mypy_message)) {
            std::string file_path = match[1].str();
            std::string line_str = match[2].str();
            std::string severity = match[3].str();
            std::string message = match[4].str();
            std::string error_code = match[5].str();
            
            int64_t line_number = 0;
            
            try {
                line_number = std::stoi(line_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            
            // Map mypy severity to ValidationEventStatus
            if (severity == "error") {
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
            } else if (severity == "warning") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity == "note") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = file_path;
            event.line_number = line_number;
            event.column_number = -1;
            event.error_code = error_code;
            event.tool_name = "mypy";
            event.category = "type_checking";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"error_code\": \"" + error_code + "\", \"severity\": \"" + severity + "\"}";
            
            events.push_back(event);
        }
        // Check for mypy message without error code
        else if (std::regex_search(line, match, mypy_message_no_code)) {
            std::string file_path = match[1].str();
            std::string line_str = match[2].str();
            std::string severity = match[3].str();
            std::string message = match[4].str();
            
            int64_t line_number = 0;
            
            try {
                line_number = std::stoi(line_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            
            // Map mypy severity to ValidationEventStatus
            if (severity == "error") {
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.severity = "error";
                event.status = ValidationEventStatus::ERROR;
            } else if (severity == "warning") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            } else if (severity == "note") {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "info";
                event.status = ValidationEventStatus::INFO;
            } else {
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.severity = "warning";
                event.status = ValidationEventStatus::WARNING;
            }
            
            event.message = message;
            event.file_path = file_path;
            event.line_number = line_number;
            event.column_number = -1;
            event.tool_name = "mypy";
            event.category = "type_checking";
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"severity\": \"" + severity + "\"}";
            
            events.push_back(event);
        }
        // Check for summary with errors
        else if (std::regex_search(line, match, mypy_summary)) {
            std::string error_count = match[1].str();
            std::string file_count = match[2].str();
            std::string checked_count = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.severity = "error";
            event.status = ValidationEventStatus::ERROR;
            event.message = "Found " + error_count + " errors in " + file_count + " files (checked " + checked_count + " files)";
            event.tool_name = "mypy";
            event.category = "type_checking";
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"error_count\": " + error_count + ", \"file_count\": " + file_count + ", \"checked_count\": " + checked_count + "}";
            
            events.push_back(event);
        }
        // Check for success message
        else if (std::regex_search(line, match, mypy_success)) {
            std::string checked_count = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = ValidationEventStatus::PASS;
            event.message = "Success: no issues found in " + checked_count + " source files";
            event.tool_name = "mypy";
            event.category = "type_checking";
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = line;
            event.structured_data = "{\"checked_count\": " + checked_count + "}";
            
            events.push_back(event);
        }
    }
    
    return events;
}

// Auto-register this parser
REGISTER_PARSER(MypyParser);

} // namespace duckdb