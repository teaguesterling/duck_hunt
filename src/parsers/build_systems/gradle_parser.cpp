#include "gradle_parser.hpp"
#include <sstream>
#include <algorithm>

namespace duckdb {

bool GradleParser::canParse(const std::string& content) const {
    return (content.find("> Task :") != std::string::npos) ||
           (content.find("BUILD SUCCESSFUL") != std::string::npos || content.find("BUILD FAILED") != std::string::npos) ||
           (content.find("gradle") != std::string::npos && content.find("tests completed") != std::string::npos) ||
           (content.find("Execution failed for task") != std::string::npos) ||
           (content.find("* What went wrong:") != std::string::npos);
}

std::vector<ValidationEvent> GradleParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Pre-compiled regex patterns
    static const std::regex task_pattern(R"(> Task :([^\s]+)\s+(FAILED|UP-TO-DATE|SKIPPED))");
    static const std::regex compile_error_pattern(R"((.+?):(\d+): error: (.+))");
    static const std::regex test_failure_pattern(R"((\w+) > (\w+) (FAILED|PASSED|SKIPPED))");
    static const std::regex test_summary_pattern(R"((\d+) tests completed(?:, (\d+) failed)?(?:, (\d+) skipped)?)");
    static const std::regex checkstyle_pattern(R"(\[ant:checkstyle\] (.+?):(\d+): (.+?) \[(.+?)\])");
    static const std::regex spotbugs_pattern(R"(Bug: (High|Medium|Low): (.+?) \[(.+?)\])");
    static const std::regex android_lint_pattern(R"((.+?):(\d+): (Error|Warning): (.+?) \[(.+?)\])");
    static const std::regex build_result_pattern(R"(BUILD (SUCCESSFUL|FAILED) in (\d+)s)");
    static const std::regex execution_failed_pattern(R"(Execution failed for task '([^']+)')");

    std::string current_task = "";

    while (std::getline(iss, line)) {
        current_line_num++;
        std::smatch match;

        if (std::regex_search(line, match, task_pattern)) {
            std::string task_name = match[1].str();
            std::string task_result = match[2].str();
            current_task = task_name;

            if (task_result == "FAILED") {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "gradle";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = task_name;
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "task_failure";
                event.message = "Task " + task_name + " failed";
                event.log_content = content;
                event.structured_data = "gradle_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;
                events.push_back(event);
            }
        }
        else if (std::regex_search(line, match, compile_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-javac";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.ref_file = match[1].str();
            event.ref_line = std::stoi(match[2].str());
            event.ref_column = -1;
            event.function_name = current_task;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[3].str();
            event.log_content = content;
            event.structured_data = "gradle_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, test_failure_pattern)) {
            std::string test_class = match[1].str();
            std::string test_method = match[2].str();
            std::string test_result = match[3].str();

            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = test_method;
            event.test_name = test_class + "." + test_method;

            if (test_result == "FAILED") {
                event.status = ValidationEventStatus::FAIL;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
            } else if (test_result == "PASSED") {
                event.status = ValidationEventStatus::PASS;
                event.severity = "info";
                event.category = "test_success";
                event.message = "Test passed";
            } else if (test_result == "SKIPPED") {
                event.status = ValidationEventStatus::SKIP;
                event.severity = "info";
                event.category = "test_skipped";
                event.message = "Test skipped";
            }

            event.log_content = content;
            event.structured_data = "gradle_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, test_summary_pattern)) {
            int total_tests = std::stoi(match[1].str());
            int failed_tests = match[2].matched ? std::stoi(match[2].str()) : 0;
            int skipped_tests = match[3].matched ? std::stoi(match[3].str()) : 0;

            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-test";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = (failed_tests > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
            event.severity = (failed_tests > 0) ? "error" : "info";
            event.category = "test_summary";
            event.message = "Tests: " + std::to_string(total_tests) +
                           " completed, " + std::to_string(failed_tests) + " failed, " +
                           std::to_string(skipped_tests) + " skipped";
            event.log_content = content;
            event.structured_data = "gradle_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, checkstyle_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-checkstyle";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.ref_file = match[1].str();
            event.ref_line = std::stoi(match[2].str());
            event.ref_column = -1;
            event.function_name = current_task;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "style";
            event.message = match[3].str();
            event.error_code = match[4].str();
            event.log_content = content;
            event.structured_data = "gradle_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, spotbugs_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-spotbugs";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.function_name = current_task;
            event.severity = match[1].str();
            std::transform(event.severity.begin(), event.severity.end(), event.severity.begin(), ::tolower);
            event.status = (event.severity == "high") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.category = "static_analysis";
            event.message = match[2].str();
            event.error_code = match[3].str();
            event.log_content = content;
            event.structured_data = "gradle_build";

            if (event.error_code.find("SQL") != std::string::npos) {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("PERFORMANCE") != std::string::npos ||
                      event.error_code.find("DLS_") != std::string::npos) {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            }
            events.push_back(event);
        }
        else if (std::regex_search(line, match, android_lint_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle-android-lint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.ref_file = match[1].str();
            event.ref_line = std::stoi(match[2].str());
            event.ref_column = -1;
            event.function_name = current_task;

            std::string level = match[3].str();
            if (level == "Error") {
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
            } else {
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
            }

            event.category = "android_lint";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.log_content = content;
            event.structured_data = "gradle_build";

            if (event.error_code.find("Security") != std::string::npos ||
                event.error_code.find("SQLInjection") != std::string::npos) {
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "security";
            } else if (event.error_code.find("Performance") != std::string::npos ||
                      event.error_code.find("ThreadSleep") != std::string::npos) {
                event.event_type = ValidationEventType::PERFORMANCE_ISSUE;
                event.category = "performance";
            }
            events.push_back(event);
        }
        else if (std::regex_search(line, match, build_result_pattern)) {
            std::string result = match[1].str();
            int duration = std::stoi(match[2].str());

            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = (result == "SUCCESSFUL") ? ValidationEventStatus::PASS : ValidationEventStatus::ERROR;
            event.severity = (result == "SUCCESSFUL") ? "info" : "error";
            event.category = "build_result";
            event.message = "Build " + result;
            event.execution_time = static_cast<double>(duration);
            event.log_content = content;
            event.structured_data = "gradle_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, execution_failed_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "gradle";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.function_name = match[1].str();
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "execution_failure";
            event.message = "Execution failed for task '" + match[1].str() + "'";
            event.log_content = content;
            event.structured_data = "gradle_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
    }

    return events;
}

} // namespace duckdb
