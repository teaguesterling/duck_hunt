#include "mocha_chai_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool MochaChaiTextParser::CanParse(const std::string& content) const {
    // Check for Mocha/Chai text patterns (should be checked before RSpec since they can share similar symbols)
    return (content.find("✓") != std::string::npos || content.find("✗") != std::string::npos) &&
           (content.find("passing") != std::string::npos || content.find("failing") != std::string::npos) &&
           (content.find("Context.<anonymous>") != std::string::npos || content.find("Test.Runnable.run") != std::string::npos ||
            content.find("AssertionError") != std::string::npos || content.find("at Context") != std::string::npos);
}

void MochaChaiTextParser::Parse(const std::string& content, std::vector<ValidationEvent>& events) const {
    ParseMochaChai(content, events);
}

void MochaChaiTextParser::ParseMochaChai(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    
    // Regex patterns for Mocha/Chai output
    std::regex test_passed(R"(\s*✓\s*(.+)\s*\((\d+)ms\))");
    std::regex test_failed(R"(\s*✗\s*(.+))");
    std::regex test_pending(R"(\s*-\s*(.+)\s*\(pending\))");
    std::regex context_start(R"(^\s*([A-Z][A-Za-z0-9\s]+)\s*$)");
    std::regex nested_context(R"(^\s{2,}([a-z][A-Za-z0-9\s]+)\s*$)");
    std::regex error_line(R"((Error|AssertionError):\s*(.+))");
    std::regex file_line(R"(\s*at\s+Context\.<anonymous>\s+\((.+):(\d+):(\d+)\))");
    std::regex test_stack(R"(\s*at\s+Test\.Runnable\.run\s+\((.+):(\d+):(\d+)\))");
    std::regex summary_line(R"(\s*(\d+)\s+passing\s*\(([0-9.]+s)\))");
    std::regex failing_line(R"(\s*(\d+)\s+failing)");
    std::regex pending_line(R"(\s*(\d+)\s+pending)");
    std::regex failed_example_start(R"(\s*(\d+)\)\s+(.+))");
    std::regex expected_got_line(R"(\s*\+(.+))");
    std::regex actual_line(R"(\s*-(.+))");
    
    std::string current_context;
    std::string current_nested_context;
    std::string current_test_name;
    std::string current_error_message;
    std::string current_file_path;
    int current_line_number = 0;
    int current_column = 0;
    int64_t current_execution_time = 0;
    std::vector<std::string> stack_trace;
    bool in_failure_details = false;
    int failure_number = 0;
    
    while (std::getline(stream, line)) {
        std::smatch match;
        
        // Check for test passed
        if (std::regex_match(line, match, test_passed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            current_execution_time = std::stoll(time_str);
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.message = "Test passed: " + test_name;
            event.test_name = current_context + " " + current_nested_context + " " + test_name;
            event.status = ValidationEventStatus::PASS;
            event.file_path = current_file_path;
            event.line_number = current_line_number;
            event.column_number = current_column;
            event.execution_time = current_execution_time;
            event.tool_name = "mocha";
            event.category = "mocha_chai_text";
            event.raw_output = line;
            event.function_name = current_context;
            event.structured_data = "{}";
            
            events.push_back(event);
            
            // Reset for next test
            current_file_path = "";
            current_line_number = 0;
            current_column = 0;
            current_execution_time = 0;
        }
        // Check for test failed
        else if (std::regex_match(line, match, test_failed)) {
            current_test_name = match[1].str();
        }
        // Check for test pending
        else if (std::regex_match(line, match, test_pending)) {
            std::string test_name = match[1].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.message = "Test pending: " + test_name;
            event.test_name = current_context + " " + current_nested_context + " " + test_name;
            event.status = ValidationEventStatus::SKIP;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "mocha";
            event.category = "mocha_chai_text";
            event.raw_output = line;
            event.function_name = current_context;
            event.structured_data = "{}";
            
            events.push_back(event);
        }
        // Check for context start
        else if (std::regex_match(line, match, context_start)) {
            current_context = match[1].str();
            current_nested_context = "";
        }
        // Check for nested context
        else if (std::regex_match(line, match, nested_context)) {
            current_nested_context = match[1].str();
        }
        // Check for error messages
        else if (std::regex_match(line, match, error_line)) {
            current_error_message = match[1].str() + ": " + match[2].str();
        }
        // Check for file and line information
        else if (std::regex_match(line, match, file_line)) {
            current_file_path = match[1].str();
            current_line_number = std::stoi(match[2].str());
            current_column = std::stoi(match[3].str());
            
            // If we have a failed test, create the event now
            if (!current_test_name.empty() && !current_error_message.empty()) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.event_type = ValidationEventType::TEST_RESULT;
                event.severity = "error";
                event.message = current_error_message;
                event.test_name = current_context + " " + current_nested_context + " " + current_test_name;
                event.status = ValidationEventStatus::FAIL;
                event.file_path = current_file_path;
                event.line_number = current_line_number;
                event.column_number = current_column;
                event.execution_time = 0;
                event.tool_name = "mocha";
                event.category = "mocha_chai_text";
                event.raw_output = line;
                event.function_name = current_context;
                event.structured_data = "{}";
                
                events.push_back(event);
                
                // Reset for next test
                current_test_name = "";
                current_error_message = "";
                current_file_path = "";
                current_line_number = 0;
                current_column = 0;
            }
        }
        // Check for failed example start (in failure summary)
        else if (std::regex_match(line, match, failed_example_start)) {
            failure_number = std::stoi(match[1].str());
            std::string full_test_name = match[2].str();
            in_failure_details = true;
            
            // Extract context and test name from full name
            size_t last_space = full_test_name.rfind(' ');
            if (last_space != std::string::npos) {
                current_context = full_test_name.substr(0, last_space);
                current_test_name = full_test_name.substr(last_space + 1);
            } else {
                current_test_name = full_test_name;
            }
        }
        // Check for summary lines
        else if (std::regex_match(line, match, summary_line)) {
            int passing_count = std::stoi(match[1].str());
            std::string total_time = match[2].str();
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "info";
            summary_event.message = "Test execution completed with " + std::to_string(passing_count) + " passing tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::INFO;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"passing_tests\": " + std::to_string(passing_count) + ", \"total_time\": \"" + total_time + "\"}";
            
            events.push_back(summary_event);
        }
        else if (std::regex_match(line, match, failing_line)) {
            int failing_count = std::stoi(match[1].str());
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "error";
            summary_event.message = "Test execution completed with " + std::to_string(failing_count) + " failing tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::FAIL;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"failing_tests\": " + std::to_string(failing_count) + "}";
            
            events.push_back(summary_event);
        }
        else if (std::regex_match(line, match, pending_line)) {
            int pending_count = std::stoi(match[1].str());
            
            ValidationEvent summary_event;
            summary_event.event_id = event_id++;
            summary_event.event_type = ValidationEventType::SUMMARY;
            summary_event.severity = "warning";
            summary_event.message = "Test execution completed with " + std::to_string(pending_count) + " pending tests";
            summary_event.test_name = "";
            summary_event.status = ValidationEventStatus::WARNING;
            summary_event.file_path = "";
            summary_event.line_number = 0;
            summary_event.column_number = 0;
            summary_event.execution_time = 0;
            summary_event.tool_name = "mocha";
            summary_event.category = "mocha_chai_text";
            summary_event.raw_output = line;
            summary_event.function_name = "";
            summary_event.structured_data = "{\"pending_tests\": " + std::to_string(pending_count) + "}";
            
            events.push_back(summary_event);
        }
        
        // Always add stack trace lines when we encounter them
        if (std::regex_match(line, match, test_stack) || std::regex_match(line, match, file_line)) {
            stack_trace.push_back(line);
        }
    }
}

} // namespace duck_hunt