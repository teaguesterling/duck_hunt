#include "nunit_xunit_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool NUnitXUnitTextParser::CanParse(const std::string& content) const {
    return content.find("NUnit") != std::string::npos ||
           content.find("xUnit.net") != std::string::npos ||
           content.find("[PASS]") != std::string::npos ||
           content.find("[FAIL]") != std::string::npos ||
           content.find("[SKIP]") != std::string::npos ||
           content.find("Test Count:") != std::string::npos ||
           content.find("Overall result:") != std::string::npos;
}

void NUnitXUnitTextParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseNUnitXUnit(content, events);
}

void NUnitXUnitTextParser::ParseNUnitXUnit(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Regex patterns for NUnit/xUnit output
    std::regex nunit_header(R"(NUnit\s+([\d\.]+))");
    std::regex nunit_summary(R"(Test Count:\s*(\d+),\s*Passed:\s*(\d+),\s*Failed:\s*(\d+),\s*Warnings:\s*(\d+),\s*Inconclusive:\s*(\d+),\s*Skipped:\s*(\d+))");
    std::regex nunit_overall_result(R"(Overall result:\s*(\w+))");
    std::regex nunit_duration(R"(Duration:\s*([\d\.]+)\s*seconds)");
    std::regex nunit_failed_test(R"(\d+\)\s*(.+))");
    std::regex nunit_test_source(R"(Source:\s*(.+):line\s*(\d+))");
    std::regex nunit_test_assertion(R"(Expected:\s*(.+)\s*But was:\s*(.+))");
    
    std::regex xunit_header(R"(xUnit\.net VSTest Adapter\s+v([\d\.]+))");
    std::regex xunit_test_start(R"(Starting:\s*(.+))");
    std::regex xunit_test_finish(R"(Finished:\s*(.+))");
    std::regex xunit_test_pass(R"(\s*(.+)\s*\[PASS\])");
    std::regex xunit_test_fail(R"(\s*(.+)\s*\[FAIL\])");
    std::regex xunit_test_skip(R"(\s*(.+)\s*\[SKIP\])");
    std::regex xunit_assertion_failure(R"(Assert\.(\w+)\(\)\s*Failure)");
    std::regex xunit_stack_trace(R"(at\s+(.+)\s+in\s+(.+):line\s+(\d+))");
    std::regex xunit_total_summary(R"(Total tests:\s*(\d+))");
    std::regex xunit_passed_summary(R"(Passed:\s*(\d+))");
    std::regex xunit_failed_summary(R"(Failed:\s*(\d+))");
    std::regex xunit_skipped_summary(R"(Skipped:\s*(\d+))");
    std::regex xunit_time_summary(R"(Time:\s*([\d\.]+)s)");
    
    std::string current_test_suite;
    std::string current_framework = "unknown";
    bool in_failed_tests_section = false;
    bool in_skipped_tests_section = false;
    bool in_xunit_test_failure = false;
    std::vector<std::string> failure_details;
    
    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;

        // Detect NUnit vs xUnit framework
        if (std::regex_search(line, match, nunit_header)) {
            current_framework = "nunit";
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.message = "NUnit version " + match[1].str();
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        else if (std::regex_search(line, match, xunit_header)) {
            current_framework = "xunit";
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.message = "xUnit.net VSTest Adapter version " + match[1].str();
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // NUnit Test Summary
        else if (std::regex_search(line, match, nunit_summary)) {
            int total_tests = std::stoi(match[1].str());
            int passed = std::stoi(match[2].str());
            int failed = std::stoi(match[3].str());
            int warnings = std::stoi(match[4].str());
            int inconclusive = std::stoi(match[5].str());
            int skipped = std::stoi(match[6].str());
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.severity = failed > 0 ? "error" : "info";
            event.status = failed > 0 ? duckdb::ValidationEventStatus::FAIL : duckdb::ValidationEventStatus::PASS;
            event.message = "Test summary: " + std::to_string(total_tests) + " total, " + 
                          std::to_string(passed) + " passed, " + std::to_string(failed) + " failed, " +
                          std::to_string(skipped) + " skipped";
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // NUnit Overall Result
        else if (std::regex_search(line, match, nunit_overall_result)) {
            std::string result = match[1].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.severity = (result == "Failed") ? "error" : "info";
            event.status = (result == "Failed") ? duckdb::ValidationEventStatus::FAIL : duckdb::ValidationEventStatus::PASS;
            event.message = "Overall test result: " + result;
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // NUnit Duration
        else if (std::regex_search(line, match, nunit_duration)) {
            double duration_seconds = std::stod(match[1].str());
            int64_t duration_ms = static_cast<int64_t>(duration_seconds * 1000);
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::PERFORMANCE_METRIC;
            event.severity = "info";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.message = "Test execution time: " + match[1].str() + " seconds";
            event.execution_time = duration_ms;
            event.tool_name = "nunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // xUnit Test Suite Start
        else if (std::regex_search(line, match, xunit_test_start)) {
            current_test_suite = match[1].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.message = "Starting test suite: " + current_test_suite;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // xUnit Test Suite Finish
        else if (std::regex_search(line, match, xunit_test_finish)) {
            std::string suite_name = match[1].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::DEBUG_INFO;
            event.severity = "info";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.message = "Finished test suite: " + suite_name;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // xUnit Test Pass
        else if (std::regex_search(line, match, xunit_test_pass)) {
            std::string test_name = match[1].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.severity = "info";
            event.status = duckdb::ValidationEventStatus::PASS;
            event.message = "Test passed: " + test_name;
            event.test_name = test_name;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // xUnit Test Fail
        else if (std::regex_search(line, match, xunit_test_fail)) {
            std::string test_name = match[1].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.severity = "error";
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.message = "Test failed: " + test_name;
            event.test_name = test_name;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // xUnit Test Skip
        else if (std::regex_search(line, match, xunit_test_skip)) {
            std::string test_name = match[1].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.severity = "warning";
            event.status = duckdb::ValidationEventStatus::SKIP;
            event.message = "Test skipped: " + test_name;
            event.test_name = test_name;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // xUnit Test Summary
        else if (std::regex_search(line, match, xunit_total_summary)) {
            int total_tests = std::stoi(match[1].str());
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::SUMMARY;
            event.severity = "info";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.message = "Total tests: " + std::to_string(total_tests);
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.execution_time = 0;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // xUnit Time Summary
        else if (std::regex_search(line, match, xunit_time_summary)) {
            double duration_seconds = std::stod(match[1].str());
            int64_t duration_ms = static_cast<int64_t>(duration_seconds * 1000);
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.event_type = duckdb::ValidationEventType::PERFORMANCE_METRIC;
            event.severity = "info";
            event.status = duckdb::ValidationEventStatus::INFO;
            event.message = "Test execution time: " + match[1].str() + " seconds";
            event.execution_time = duration_ms;
            event.tool_name = "xunit";
            event.category = "nunit_xunit_text";
            event.raw_output = line;
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Handle file path and line number information
        else if (std::regex_search(line, match, nunit_test_source)) {
            std::string file_path = match[1].str();
            int line_number = std::stoi(match[2].str());
            
            // Update the most recent test event with file/line info
            if (!events.empty()) {
                duckdb::ValidationEvent& last_event = events.back();
                if (last_event.category == "nunit_xunit_text" && last_event.file_path.empty()) {
                    last_event.file_path = file_path;
                    last_event.line_number = line_number;
                }
            }
        }
        // Handle xUnit stack traces
        else if (std::regex_search(line, match, xunit_stack_trace)) {
            std::string file_path = match[2].str();
            int line_number = std::stoi(match[3].str());
            
            // Update the most recent test event with file/line info
            if (!events.empty()) {
                duckdb::ValidationEvent& last_event = events.back();
                if (last_event.category == "nunit_xunit_text" && last_event.file_path.empty()) {
                    last_event.file_path = file_path;
                    last_event.line_number = line_number;
                }
            }
        }
    }
}

} // namespace duck_hunt