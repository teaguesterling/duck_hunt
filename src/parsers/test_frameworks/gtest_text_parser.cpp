#include "gtest_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool GTestTextParser::CanParse(const std::string& content) const {
    return content.find("[RUN      ]") != std::string::npos ||
           content.find("[       OK ]") != std::string::npos ||
           content.find("[  FAILED  ]") != std::string::npos ||
           content.find("[==========]") != std::string::npos ||
           content.find("[----------]") != std::string::npos;
}

void GTestTextParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseGoogleTest(content, events);
}

void GTestTextParser::ParseGoogleTest(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Regex patterns for Google Test output
    std::regex test_run_start(R"(\[\s*RUN\s*\]\s*(.+))");
    std::regex test_passed(R"(\[\s*OK\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_failed(R"(\[\s*FAILED\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_skipped(R"(\[\s*SKIPPED\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
    std::regex test_suite_start(R"(\[----------\]\s*(\d+)\s*tests from\s*(.+))");
    std::regex test_suite_end(R"(\[----------\]\s*(\d+)\s*tests from\s*(.+)\s*\((\d+)\s*ms total\))");
    std::regex test_summary_start(R"(\[==========\]\s*(\d+)\s*tests from\s*(\d+)\s*test suites ran\.\s*\((\d+)\s*ms total\))");
    std::regex tests_passed_summary(R"(\[\s*PASSED\s*\]\s*(\d+)\s*tests\.)");
    std::regex tests_failed_summary(R"(\[\s*FAILED\s*\]\s*(\d+)\s*tests,\s*listed below:)");
    std::regex failed_test_list(R"(\[\s*FAILED\s*\]\s*(.+))");
    std::regex failure_detail(R"((.+):\s*(.+):(\d+):\s*Failure)");
    std::regex global_env_setup(R"(\[----------\]\s*Global test environment set-up)");
    std::regex global_env_teardown(R"(\[----------\]\s*Global test environment tear-down)");
    
    std::string current_test_suite;
    std::string current_test_name;
    bool in_failure_details = false;
    std::vector<std::string> failure_lines;
    
    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;

        // Check for test run start
        if (std::regex_match(line, match, test_run_start)) {
            current_test_name = match[1].str();
        }
        // Check for test passed
        else if (std::regex_match(line, match, test_passed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.message = "Test passed: " + test_name;
            event.test_name = test_name;
            event.status = duckdb::ValidationEventStatus::PASS;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Check for test failed
        else if (std::regex_match(line, match, test_failed)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.message = "Test failed: " + test_name;
            event.test_name = test_name;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Check for test skipped
        else if (std::regex_match(line, match, test_skipped)) {
            std::string test_name = match[1].str();
            std::string time_str = match[2].str();
            int64_t execution_time = std::stoll(time_str);
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.message = "Test skipped: " + test_name;
            event.test_name = test_name;
            event.status = duckdb::ValidationEventStatus::SKIP;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = execution_time;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{}";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Check for test suite start
        else if (std::regex_match(line, match, test_suite_start)) {
            current_test_suite = match[2].str();
        }
        // Check for test suite end
        else if (std::regex_match(line, match, test_suite_end)) {
            std::string suite_name = match[2].str();
            std::string total_time = match[3].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Test suite completed: " + suite_name + " (" + total_time + " ms total)";
            event.test_name = "";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = std::stoll(total_time);
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = suite_name;
            event.structured_data = "{\"suite_name\": \"" + suite_name + "\", \"total_time_ms\": " + total_time + "}";
            
            events.push_back(event);
        }
        // Check for overall test summary
        else if (std::regex_match(line, match, test_summary_start)) {
            std::string total_tests = match[1].str();
            std::string total_suites = match[2].str();
            std::string total_time = match[3].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Test execution completed: " + total_tests + " tests from " + total_suites + " test suites";
            event.test_name = "";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = std::stoll(total_time);
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"total_tests\": " + total_tests + ", \"total_suites\": " + total_suites + ", \"total_time_ms\": " + total_time + "}";
            
            events.push_back(event);
        }
        // Check for passed tests summary
        else if (std::regex_match(line, match, tests_passed_summary)) {
            std::string passed_count = match[1].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.severity = "info";
            event.message = "Tests passed: " + passed_count + " tests";
            event.test_name = "";
            event.status = duckdb::ValidationEventStatus::PASS;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"passed_tests\": " + passed_count + "}";
            
            events.push_back(event);
        }
        // Check for failed tests summary
        else if (std::regex_match(line, match, tests_failed_summary)) {
            std::string failed_count = match[1].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.severity = "error";
            event.message = "Tests failed: " + failed_count + " tests";
            event.test_name = "";
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.file_path = "";
            event.line_number = 0;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = "";
            event.structured_data = "{\"failed_tests\": " + failed_count + "}";
            
            events.push_back(event);
        }
        // Check for failure details (file paths and line numbers)
        else if (std::regex_match(line, match, failure_detail)) {
            std::string test_name = match[1].str();
            std::string file_path = match[2].str();
            std::string line_str = match[3].str();
            int line_number = 0;
            
            try {
                line_number = std::stoi(line_str);
            } catch (...) {
                // If parsing line number fails, keep it as 0
            }
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.message = "Test failure details: " + test_name;
            event.test_name = test_name;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.file_path = file_path;
            event.line_number = line_number;
            event.column_number = 0;
            event.execution_time = 0;
            event.tool_name = "gtest";
            event.category = "gtest_text";
            event.raw_output = line;
            event.function_name = current_test_suite;
            event.structured_data = "{\"file_path\": \"" + file_path + "\", \"line_number\": " + std::to_string(line_number) + "}";
            
            events.push_back(event);
        }
    }
}

} // namespace duck_hunt