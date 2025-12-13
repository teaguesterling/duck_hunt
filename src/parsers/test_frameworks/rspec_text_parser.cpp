#include "rspec_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool RSpecTextParser::CanParse(const std::string& content) const {
    // Check for RSpec text patterns (should be checked after Mocha/Chai since they can contain similar keywords)
    return (content.find("✓") != std::string::npos || content.find("✗") != std::string::npos) &&
           (content.find("examples") != std::string::npos || content.find("failures") != std::string::npos) &&
           (content.find("rspec") != std::string::npos || content.find("Failure/Error:") != std::string::npos);
}

void RSpecTextParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseRSpecText(content, events);
}

void RSpecTextParser::ParseRSpecText(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Regex patterns for RSpec output
    std::regex test_passed(R"(\s*✓\s*(.+))");
    std::regex test_failed(R"(\s*✗\s*(.+))");
    std::regex test_pending(R"(\s*pending:\s*(.+)\s*\(PENDING:\s*(.+)\))");
    std::regex context_start(R"(^([A-Z][A-Za-z0-9_:]+)\s*$)");
    std::regex nested_context(R"(^\s+(#\w+|.+)\s*$)");
    std::regex failure_error(R"(Failure/Error:\s*(.+))");
    std::regex expected_pattern(R"(\s*expected\s*(.+))");
    std::regex got_pattern(R"(\s*got:\s*(.+))");
    std::regex file_line_pattern(R"(# (.+):(\d+):in)");
    std::regex summary_pattern(R"(Finished in (.+) seconds .* (\d+) examples?, (\d+) failures?(, (\d+) pending)?)");;
    std::regex failed_example(R"(rspec (.+):(\d+) # (.+))");
    
    std::string current_context;
    std::string current_method;
    std::string current_failure_file;
    int current_failure_line = -1;
    std::string current_failure_message;
    bool in_failure_section = false;
    bool in_failed_examples = false;
    
    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;

        // Skip empty lines and dividers
        if (line.empty() || line.find("Failures:") != std::string::npos ||
            line.find("Failed examples:") != std::string::npos) {
            if (line.find("Failures:") != std::string::npos) {
                in_failure_section = true;
            }
            if (line.find("Failed examples:") != std::string::npos) {
                in_failed_examples = true;
            }
            continue;
        }
        
        // Parse failed example references
        if (in_failed_examples && std::regex_search(line, match, failed_example)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.test_name = match[3].str();
            event.message = "Test failed: " + match[3].str();
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
            continue;
        }
        
        // Parse test context (class/module names)
        if (std::regex_match(line, match, context_start)) {
            current_context = match[1].str();
            continue;
        }
        
        // Parse nested context (method names or descriptions)
        if (!current_context.empty() && std::regex_match(line, match, nested_context)) {
            current_method = match[1].str();
            // Remove leading # if present
            if (current_method.substr(0, 1) == "#") {
                current_method = current_method.substr(1);
            }
            continue;
        }
        
        // Parse passed tests
        if (std::regex_search(line, match, test_passed)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "test_success";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test passed: " + match[1].str();
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Parse failed tests
        else if (std::regex_search(line, match, test_failed)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.severity = "error";
            event.category = "test_failure";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test failed: " + match[1].str();
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Parse pending tests
        else if (std::regex_search(line, match, test_pending)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::SKIP;
            event.severity = "warning";
            event.category = "test_pending";
            if (!current_context.empty()) {
                event.function_name = current_context;
                if (!current_method.empty()) {
                    event.function_name += "::" + current_method;
                }
            }
            event.test_name = match[1].str();
            event.message = "Test pending: " + match[2].str();
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        
        // Parse failure details
        else if (std::regex_search(line, match, failure_error)) {
            current_failure_message = match[1].str();
        }
        
        // Parse expected/got patterns for better error details
        else if (std::regex_search(line, match, expected_pattern)) {
            if (!current_failure_message.empty()) {
                current_failure_message += " | Expected: " + match[1].str();
            }
        }
        else if (std::regex_search(line, match, got_pattern)) {
            if (!current_failure_message.empty()) {
                current_failure_message += " | Got: " + match[1].str();
            }
        }
        
        // Parse file and line information
        else if (std::regex_search(line, match, file_line_pattern)) {
            current_failure_file = match[1].str();
            current_failure_line = std::stoi(match[2].str());
            
            // Update the most recent failed test event with file/line info
            for (auto it = events.rbegin(); it != events.rend(); ++it) {
                if (it->tool_name == "RSpec" && it->status == duckdb::ValidationEventStatus::FAIL && 
                    it->file_path.empty()) {
                    it->file_path = current_failure_file;
                    it->line_number = current_failure_line;
                    if (!current_failure_message.empty()) {
                        it->message = current_failure_message;
                    }
                    break;
                }
            }
        }
        
        // Parse summary
        else if (std::regex_search(line, match, summary_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "RSpec";
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.status = duckdb::ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_summary";
            
            std::string execution_time = match[1].str();
            int total_examples = std::stoi(match[2].str());
            int failures = std::stoi(match[3].str());
            int pending = 0;
            if (match[5].matched) {
                pending = std::stoi(match[5].str());
            }
            
            event.message = "Test run completed: " + std::to_string(total_examples) + 
                          " examples, " + std::to_string(failures) + " failures, " + 
                          std::to_string(pending) + " pending";
            event.execution_time = std::stod(execution_time);
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
    }
}

} // namespace duck_hunt