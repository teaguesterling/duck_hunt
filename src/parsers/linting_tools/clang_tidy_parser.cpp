#include "clang_tidy_parser.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

bool ClangTidyParser::canParse(const std::string& content) const {
    // Check for clang-tidy specific patterns with high priority
    if (isValidClangTidyOutput(content)) {
        return true;
    }
    return false;
}

bool ClangTidyParser::isValidClangTidyOutput(const std::string& content) const {
    // Look for clang-tidy specific patterns that distinguish it from MyPy
    // clang-tidy format: file:line:column: error/warning: message [rule-name]
    // MyPy format: file:line: error/warning: message [error-code]
    
    // Check for clang-tidy specific rule names in square brackets
    std::vector<std::string> clang_tidy_rules = {
        "readability-", "performance-", "modernize-", "bugprone-", 
        "cppcoreguidelines-", "google-", "llvm-", "misc-", "portability-",
        "hicpp-", "cert-", "fuchsia-", "abseil-", "android-", "boost-", 
        "darwin-", "linuxkernel-", "mpi-", "objc-", "openmp-", "zircon-",
        "clang-analyzer-", "concurrency-", "altera-", "readability-braces-around-statements",
        "bugprone-narrowing-conversions", "cppcoreguidelines-avoid-non-const-global-variables",
        "google-build-using-namespace"
    };
    
    // First check if we have any clang-tidy specific rules
    for (const auto& rule : clang_tidy_rules) {
        if (content.find(rule) != std::string::npos) {
            return true;
        }
    }
    
    // Check for clang-tidy specific output patterns
    // clang-tidy often includes column numbers (file:line:column:)
    // and has different message patterns than mypy
    // Guard: only run expensive regex if we have error/warning/note patterns
    if (content.find(": error:") != std::string::npos ||
        content.find(": warning:") != std::string::npos ||
        content.find(": note:") != std::string::npos) {
        // Process line-by-line to avoid catastrophic backtracking on large files
        std::regex clang_tidy_pattern(R"([^:]+:\d+:\d+:\s*(error|warning|note):\s*[^\[]*\[[^\]]+\])");
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            if (std::regex_search(line, clang_tidy_pattern)) {
                // Additional validation: check if it looks like clang-tidy vs mypy
                // clang-tidy messages often contain C++ specific terms
                std::vector<std::string> cpp_terms = {
                    "function", "variable", "parameter", "struct", "class",
                    "namespace", "const", "static", "inline", "template", "typename"
                };

                for (const auto& term : cpp_terms) {
                    if (content.find(term) != std::string::npos) {
                        return true;
                    }
                }
            }
        }
    }
    
    // Check for clang-tidy summary patterns
    if (content.find("clang-tidy") != std::string::npos ||
        content.find("warnings generated") != std::string::npos ||
        content.find("errors generated") != std::string::npos) {
        return true;
    }
    
    // Additional patterns from main detection logic
    if ((content.find("google-") != std::string::npos && content.find("build") != std::string::npos)) {
        return true;
    }
    
    return false;
}

std::vector<ValidationEvent> ClangTidyParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Regex patterns for clang-tidy output
    std::regex clang_tidy_message(R"(([^:]+):(\d+):(\d+):\s*(error|warning|note):\s*(.+?)\s*\[([^\]]+)\])");
    std::regex clang_tidy_message_no_col(R"(([^:]+):(\d+):\s*(error|warning|note):\s*(.+?)\s*\[([^\]]+)\])");
    std::regex clang_tidy_summary(R"((\d+)\s+(warnings?|errors?)\s+generated)");

    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;
        
        // Check for clang-tidy message with column number
        if (std::regex_search(line, match, clang_tidy_message)) {
            std::string file_path = match[1].str();
            std::string line_str = match[2].str();
            std::string column_str = match[3].str();
            std::string severity = match[4].str();
            std::string message = match[5].str();
            std::string rule_name = match[6].str();
            
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
            
            // Map clang-tidy severity to ValidationEventStatus
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
            event.ref_file = file_path;
            event.ref_line = line_number;
            event.ref_column = column_number;
            event.error_code = rule_name;
            event.tool_name = "clang-tidy";
            event.category = "static_analysis";
            event.execution_time = 0.0;
            event.log_content = line;
            event.structured_data = "{\"rule\": \"" + rule_name + "\", \"severity\": \"" + severity + "\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Check for clang-tidy message without column number
        else if (std::regex_search(line, match, clang_tidy_message_no_col)) {
            std::string file_path = match[1].str();
            std::string line_str = match[2].str();
            std::string severity = match[3].str();
            std::string message = match[4].str();
            std::string rule_name = match[5].str();
            
            int64_t line_number = 0;
            
            try {
                line_number = std::stoi(line_str);
            } catch (...) {
                // If parsing fails, keep as 0
            }
            
            ValidationEvent event;
            event.event_id = event_id++;
            
            // Map clang-tidy severity to ValidationEventStatus
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
            event.ref_file = file_path;
            event.ref_line = line_number;
            event.ref_column = -1;
            event.error_code = rule_name;
            event.tool_name = "clang-tidy";
            event.category = "static_analysis";
            event.execution_time = 0.0;
            event.log_content = line;
            event.structured_data = "{\"rule\": \"" + rule_name + "\", \"severity\": \"" + severity + "\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Check for summary
        else if (std::regex_search(line, match, clang_tidy_summary)) {
            std::string count = match[1].str();
            std::string type = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::SUMMARY;
            event.severity = type.find("error") != std::string::npos ? "error" : "warning";
            event.status = type.find("error") != std::string::npos ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.message = count + " " + type + " generated by clang-tidy";
            event.tool_name = "clang-tidy";
            event.category = "static_analysis";
            event.ref_line = -1;
            event.ref_column = -1;
            event.execution_time = 0.0;
            event.log_content = line;
            event.structured_data = "{\"count\": " + count + ", \"type\": \"" + type + "\"}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
    }

    return events;
}

} // namespace duckdb