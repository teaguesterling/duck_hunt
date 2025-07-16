#include "gradle_parser.hpp"
#include <regex>
#include <sstream>
#include <string>
#include <algorithm>

namespace duck_hunt {

bool GradleParser::CanParse(const std::string& content) const {
    // Check for Gradle patterns
    return (content.find("> Task :") != std::string::npos) ||
           (content.find("BUILD SUCCESSFUL") != std::string::npos || content.find("BUILD FAILED") != std::string::npos) ||
           (content.find("gradle") != std::string::npos && content.find("tests completed") != std::string::npos) ||
           (content.find("Execution failed for task") != std::string::npos) ||
           (content.find("* What went wrong:") != std::string::npos);
}

void GradleParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseGradleBuild(content, events);
}

void GradleParser::ParseGradleBuild(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    
    // Gradle patterns
    std::regex task_pattern(R"(> Task :([^\s]+)\s+(FAILED|UP-TO-DATE|SKIPPED))");
    std::regex compile_error_pattern(R"((.+?):(\d+): error: (.+))");
    std::regex test_failure_pattern(R"((\w+) > (\w+) (FAILED|PASSED|SKIPPED))");
    std::regex test_summary_pattern(R"((\d+) tests completed(?:, (\d+) failed)?(?:, (\d+) skipped)?)");
    std::regex checkstyle_pattern(R"(\[ant:checkstyle\] (.+?):(\d+): (.+?) \[(.+?)\])");
    std::regex spotbugs_pattern(R"(Bug: (High|Medium|Low): (.+?) \[(.+?)\])");
    std::regex android_lint_pattern(R"((.+?):(\d+): (Error|Warning): (.+?) \[(.+?)\])");
    std::regex build_result_pattern(R"(BUILD (SUCCESSFUL|FAILED) in (\d+)s)");
    std::regex execution_failed_pattern(R"(Execution failed for task '([^']+)')");
    std::regex gradle_error_pattern(R"(\* What went wrong:)");
    
    std::string current_task = "";
    bool in_error_block = false;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        // Parse task execution results
        if (std::regex_search(line, match, task_pattern)) {
            std::string task_name = match[1].str();
            std::string task_result = match[2].str();
            current_task = task_name;
            
            if (task_result == "FAILED") {
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "gradle";
                event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
                event.function_name = task_name;
                event.status = duckdb::ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "task_failure";
                event.message = "Task " + task_name + " failed";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "gradle_build";
                
                events.push_back(event);
            }
        }
        // Parse Java compilation errors
        else if (std::regex_search(line, match, compile_error_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-javac";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[3].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse test results
        else if (std::regex_search(line, match, test_failure_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string test_result = match[3].str();
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;
            
            if (test_result == "FAILED") {
                event.status = duckdb::ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (test_result == "PASSED") {
                event.status = duckdb::ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (test_result == "SKIPPED") {
                event.status = duckdb::ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }
            
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse test summaries
        else if (std::regex_search(line, match, test_summary_pattern)) {
            int total_tests = std::stoi(match[1].str());
            int failed_tests = match[2].matched ? std::stoi(match[2].str()) : 0;
            int skipped_tests = match[3].matched ? std::stoi(match[3].str()) : 0;
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = (failed_tests > 0) ? duckdb::ValidationEventStatus::FAIL : duckdb::ValidationEventStatus::PASS;
            event.severity = (failed_tests > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total_tests) + 
                           " completed, " + std::to_string(failed_tests) + " failed, " + 
                           std::to_string(skipped_tests) + " skipped";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse Checkstyle violations (Gradle format)
        else if (std::regex_search(line, match, checkstyle_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-checkstyle";
            event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            event.status = duckdb::ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "style";
            event.message = match[3].str();
            event.error_code = match[4].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse SpotBugs findings (Gradle format)
        else if (std::regex_search(line, match, spotbugs_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-spotbugs";
            event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
            event.function_name = current_task;
            event.severity = match[1].str();
            std::transform(event.severity.begin(), event.severity.end(), event.severity.begin(), ::tolower);
            event.status = (event.severity == "high") ? duckdb::ValidationEventStatus::ERROR : duckdb::ValidationEventStatus::WARNING;
            event.category = "static_analysis";
            event.message = match[2].str();
            event.error_code = match[3].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            // Map SpotBugs categories
            if (event.error_code.find("SQL") != std::string::npos) {
                event.event_type = duckdb::ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("PERFORMANCE") != std::string::npos || 
                      event.error_code.find("DLS_") != std::string::npos) {
                event.event_type = duckdb::ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            } else {
                event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
            }
            
            events.push_back(event);
        }
        // Parse Android Lint issues
        else if (std::regex_search(line, match, android_lint_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-android-lint";
            event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[2].str());
            event.column_number = -1;
            event.function_name = current_task;
            
            std::string level = match[3].str();
            if (level == "Error") {
                event.status = duckdb::ValidationEventStatus::ERROR;
                event.severity = "error";
            } else {
                event.status = duckdb::ValidationEventStatus::WARNING;
                event.severity = "warning";
            }
            
            event.category = "android_lint";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            // Map Android-specific categories
            if (event.error_code.find("Security") != std::string::npos || 
                event.error_code.find("SQLInjection") != std::string::npos) {
                event.event_type = duckdb::ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("Performance") != std::string::npos ||
                      event.error_code.find("ThreadSleep") != std::string::npos) {
                event.event_type = duckdb::ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            }
            
            events.push_back(event);
        }
        // Parse build results
        else if (std::regex_search(line, match, build_result_pattern)) {
            std::string result = match[1].str();
            int duration = std::stoi(match[2].str());
            
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = (result == "SUCCESSFUL") ? duckdb::ValidationEventStatus::PASS : duckdb::ValidationEventStatus::ERROR;
            event.severity = (result == "SUCCESSFUL") ? "info" : "error";
            event.category = "build_result";
            event.message = "Build " + result;
            event.execution_time = static_cast<double>(duration);
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Parse execution failure messages
        else if (std::regex_search(line, match, execution_failed_pattern)) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.function_name = match[1].str();
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "execution_failure";
            event.message = "Execution failed for task '" + match[1].str() + "'";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "gradle_build";
            
            events.push_back(event);
        }
        // Track error block context
        else if (std::regex_search(line, match, gradle_error_pattern)) {
            in_error_block = true;
        }
    }
}

} // namespace duck_hunt