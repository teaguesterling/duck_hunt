#include "junit_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

bool JUnitTextParser::canParse(const std::string& content) const {
    // Check for JUnit text patterns (should be checked before pytest since they can contain similar keywords)
    return content.find("Running ") != std::string::npos && 
           (content.find("Tests run:") != std::string::npos || 
            content.find("JUnit Jupiter") != std::string::npos ||
            content.find(">>> ") != std::string::npos ||
            content.find("<<< ") != std::string::npos ||
            content.find("[INFO] Running") != std::string::npos);
}

// Forward declaration for implementation
static void parseJUnitTextImpl(const std::string& content, std::vector<ValidationEvent>& events);

std::vector<ValidationEvent> JUnitTextParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    parseJUnitTextImpl(content, events);
    return events;
}

static void parseJUnitTextImpl(const std::string& content, std::vector<ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // JUnit text patterns for different output formats
    std::regex junit4_class_pattern(R"(Running (.+))");
    std::regex junit4_summary_pattern(R"(Tests run: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+), Time elapsed: ([\d.]+) sec.*?)");
    std::regex junit4_test_pattern(R"((.+?)\((.+?)\)\s+Time elapsed: ([\d.]+) sec\s+<<< (PASSED!|FAILURE!|ERROR!|SKIPPED!))");
    std::regex junit4_exception_pattern(R"((.+?): (.+)$)");
    std::regex junit4_stack_trace_pattern(R"(\s+at (.+?)\.(.+?)\((.+?):(\d+)\))");
    
    // JUnit 5 patterns
    std::regex junit5_header_pattern(R"(JUnit Jupiter ([\d.]+))");
    std::regex junit5_class_pattern(R"([├└]─ (.+?) [✓✗↷])");
    std::regex junit5_test_pattern(R"([│\s]+[├└]─ (.+?)\(\) ([✓✗↷]) \((\d+)ms\))");
    std::regex junit5_summary_pattern(R"(\[\s+(\d+) tests (found|successful|failed|skipped)\s+\])");
    
    // Maven Surefire patterns
    std::regex surefire_class_pattern(R"(\[INFO\] Running (.+))");
    std::regex surefire_test_pattern(R"(\[ERROR\] (.+?)\((.+?)\)\s+Time elapsed: ([\d.]+) s\s+<<< (FAILURE!|ERROR!))");
    std::regex surefire_summary_pattern(R"(\[INFO\] Tests run: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+))");
    std::regex surefire_results_pattern(R"(\[ERROR\] (.+):(\d+) (.+))");
    
    // Gradle patterns
    std::regex gradle_test_pattern(R"((.+?) > (.+?) (PASSED|FAILED|SKIPPED))");
    std::regex gradle_summary_pattern(R"((\d+) tests completed, (\d+) failed, (\d+) skipped)");
    
    // TestNG patterns
    std::regex testng_test_pattern(R"((.+?)\.(.+?): (PASS|FAIL|SKIP))");
    std::regex testng_summary_pattern(R"(Total tests run: (\d+), Failures: (\d+), Skips: (\d+))");
    
    std::string current_class = "";
    std::string current_exception = "";
    std::string current_test = "";
    bool in_stack_trace = false;
    
    while (std::getline(iss, line)) {
        current_line_num++;
        std::smatch match;

        // Parse JUnit 4 class execution
        if (std::regex_search(line, match, junit4_class_pattern)) {
            current_class = match[1].str();
            in_stack_trace = false;
        }
        // Parse JUnit 4 class summary
        else if (std::regex_search(line, match, junit4_summary_pattern)) {
            int tests_run = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            double time_elapsed = std::stod(match[5].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit4";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = current_class;
            event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failures > 0 || errors > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(tests_run) + 
                           " total, " + std::to_string(tests_run - failures - errors - skipped) + " passed, " +
                           std::to_string(failures) + " failed, " + 
                           std::to_string(errors) + " errors, " +
                           std::to_string(skipped) + " skipped";
            event.execution_time = time_elapsed;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse JUnit 4 individual test results
        else if (std::regex_search(line, match, junit4_test_pattern)) {
            std::string test_method = match[1].str();
            std::string test_class = match[2].str();
            double time_elapsed = std::stod(match[3].str());
            std::string result = match[4].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit4";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = time_elapsed;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            if (result == "PASSED!") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAILURE!") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
                current_test = event.test_name;
                in_stack_trace = true;
            } else if (result == "ERROR!") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_error";
                event.message = "Test error";
                current_test = event.test_name;
                in_stack_trace = true;
            } else if (result == "SKIPPED!") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse JUnit 5 header
        else if (std::regex_search(line, match, junit5_header_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit5";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "test_framework";
            event.message = "JUnit Jupiter " + match[1].str();
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse JUnit 5 class results
        else if (std::regex_search(line, match, junit5_class_pattern)) {
            current_class = match[1].str();
        }
        // Parse JUnit 5 test results
        else if (std::regex_search(line, match, junit5_test_pattern)) {
            std::string test_method = match[1].str();
            std::string result_symbol = match[2].str();
            int time_ms = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "junit5";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = current_class + "." + test_method;
            event.execution_time = static_cast<double>(time_ms) / 1000.0;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            if (result_symbol == "✓") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result_symbol == "✗") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result_symbol == "↷") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse Maven Surefire class execution
        else if (std::regex_search(line, match, surefire_class_pattern)) {
            current_class = match[1].str();
        }
        // Parse Maven Surefire test failures
        else if (std::regex_search(line, match, surefire_test_pattern)) {
            std::string test_method = match[1].str();
            std::string test_class = match[2].str();
            double time_elapsed = std::stod(match[3].str());
            std::string result = match[4].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = time_elapsed;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            if (result == "FAILURE!") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "ERROR!") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_error";
                event.message = "Test error";
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse Maven Surefire summary
        else if (std::regex_search(line, match, surefire_summary_pattern)) {
            int tests_run = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failures > 0 || errors > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(tests_run) + 
                           " total, " + std::to_string(tests_run - failures - errors - skipped) + " passed, " +
                           std::to_string(failures) + " failed, " + 
                           std::to_string(errors) + " errors, " +
                           std::to_string(skipped) + " skipped";
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse Gradle test results
        else if (std::regex_search(line, match, gradle_test_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            if (result == "PASSED") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAILED") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "SKIPPED") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse Gradle test summary
        else if (std::regex_search(line, match, gradle_summary_pattern)) {
            int total = std::stoi(match[1].str());
            int failed = std::stoi(match[2].str());
            int skipped = std::stoi(match[3].str());
            int passed = total - failed - skipped;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total) + 
                           " total, " + std::to_string(passed) + " passed, " +
                           std::to_string(failed) + " failed, " + 
                           std::to_string(skipped) + " skipped";
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse TestNG test results
        else if (std::regex_search(line, match, testng_test_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string result = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "testng";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            if (result == "PASS") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (result == "FAIL") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (result == "SKIP") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse TestNG summary
        else if (std::regex_search(line, match, testng_summary_pattern)) {
            int total = std::stoi(match[1].str());
            int failed = std::stoi(match[2].str());
            int skipped = std::stoi(match[3].str());
            int passed = total - failed - skipped;
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "testng";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total) + 
                           " total, " + std::to_string(passed) + " passed, " +
                           std::to_string(failed) + " failed, " + 
                           std::to_string(skipped) + " skipped";
            event.execution_time = 0.0;
            event.log_content = content;
            event.structured_data = "junit";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Handle exception messages in stack traces
        else if (in_stack_trace) {
            if (std::regex_search(line, match, junit4_exception_pattern)) {
                current_exception = match[1].str() + ": " + match[2].str();
            } else if (std::regex_search(line, match, junit4_stack_trace_pattern)) {
                // Extract file and line info from stack trace
                std::string file = match[3].str();
                int line_number = std::stoi(match[4].str());
                
                // Update the last test event with exception details
                if (!events.empty() && events.back().test_name == current_test) {
                    ValidationEvent& last_event = events.back();
                    last_event.ref_file = file;
                    last_event.ref_line = line_number;
                    if (!current_exception.empty()) {
                        last_event.message = current_exception;
                    }
                }
            }
        }
    }
}

} // namespace duckdb