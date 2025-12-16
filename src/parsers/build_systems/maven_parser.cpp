#include "maven_parser.hpp"
#include <sstream>
#include <algorithm>

namespace duckdb {

bool MavenParser::canParse(const std::string& content) const {
    return (content.find("[INFO]") != std::string::npos && content.find("maven") != std::string::npos) ||
           (content.find("[ERROR]") != std::string::npos && content.find("BUILD FAILURE") != std::string::npos) ||
           (content.find("Tests run:") != std::string::npos && content.find("Failures:") != std::string::npos) ||
           (content.find("maven-compiler-plugin") != std::string::npos) ||
           (content.find("maven-surefire-plugin") != std::string::npos) ||
           (content.find("maven-checkstyle-plugin") != std::string::npos);
}

std::vector<ValidationEvent> MavenParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream iss(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Pre-compiled regex patterns
    static const std::regex compile_error_pattern(R"(\[ERROR\]\s+(.+?):(\[(\d+),(\d+)\])\s+(.+))");
    static const std::regex compile_warning_pattern(R"(\[WARNING\]\s+(.+?):(\[(\d+),(\d+)\])\s+(.+))");
    static const std::regex test_failure_pattern(R"(\[ERROR\]\s+(.+?)\(\s*(.+?)\s*\)\s+Time elapsed:\s+([\d.]+)\s+s\s+<<<\s+(FAILURE|ERROR)!)");
    static const std::regex test_result_pattern(R"(Tests run:\s+(\d+),\s+Failures:\s+(\d+),\s+Errors:\s+(\d+),\s+Skipped:\s+(\d+))");
    static const std::regex checkstyle_pattern(R"(\[(ERROR|WARN)\]\s+(.+?):(\d+):\s+(.+?)\s+\[(.+?)\])");
    static const std::regex spotbugs_pattern(R"(\[(ERROR|WARN)\]\s+(High|Medium|Low):\s+(.+?)\s+in\s+(.+?)\s+\[(.+?)\])");
    static const std::regex pmd_pattern(R"(\[(ERROR|WARN)\]\s+(.+?):(\d+):\s+(.+?)\s+\[(.+?)\])");
    static const std::regex dependency_pattern(R"(\[WARNING\]\s+(Used undeclared dependencies|Unused declared dependencies) found:)");
    static const std::regex build_failure_pattern(R"(BUILD FAILURE)");

    while (std::getline(iss, line)) {
        current_line_num++;
        std::smatch match;

        if (std::regex_search(line, match, compile_error_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-compiler";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = std::stoi(match[4].str());
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "compilation";
            event.message = match[5].str();
            event.raw_output = content;
            event.structured_data = "maven_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, compile_warning_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-compiler";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.file_path = match[1].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = std::stoi(match[4].str());
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "compilation";
            event.message = match[5].str();
            event.raw_output = content;
            event.structured_data = "maven_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, test_failure_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-surefire";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.function_name = match[1].str();
            event.test_name = match[2].str() + "." + match[1].str();
            event.execution_time = std::stod(match[3].str());
            event.status = (match[4].str() == "FAILURE") ? ValidationEventStatus::FAIL : ValidationEventStatus::ERROR;
            event.severity = (match[4].str() == "FAILURE") ? "error" : "critical";
            event.category = (match[4].str() == "FAILURE") ? "test_failure" : "test_error";
            std::string failure_type = match[4].str();
            std::transform(failure_type.begin(), failure_type.end(), failure_type.begin(), ::tolower);
            event.message = "Test " + failure_type;
            event.raw_output = content;
            event.structured_data = "maven_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, checkstyle_pattern) &&
                 (content.find("maven-checkstyle-plugin") != std::string::npos ||
                  content.find("checkstyle") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "checkstyle";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[2].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = -1;
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = (match[1].str() == "ERROR") ? "error" : "warning";
            event.category = "style";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.raw_output = content;
            event.structured_data = "maven_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, spotbugs_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "spotbugs";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.function_name = match[4].str();
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = match[2].str();
            std::transform(event.severity.begin(), event.severity.end(), event.severity.begin(), ::tolower);
            event.category = "static_analysis";
            event.message = match[3].str();
            event.error_code = match[5].str();
            event.raw_output = content;
            event.structured_data = "maven_build";

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
        else if (std::regex_search(line, match, pmd_pattern) &&
                 (content.find("maven-pmd-plugin") != std::string::npos ||
                  content.find("PMD version") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pmd";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = match[2].str();
            event.line_number = std::stoi(match[3].str());
            event.column_number = -1;
            event.status = (match[1].str() == "ERROR") ? ValidationEventStatus::ERROR : ValidationEventStatus::WARNING;
            event.severity = (match[1].str() == "ERROR") ? "error" : "warning";
            event.category = "code_quality";
            event.message = match[4].str();
            event.error_code = match[5].str();
            event.raw_output = content;
            event.structured_data = "maven_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, dependency_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven-dependency";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "dependency";
            event.message = match[1].str();
            event.raw_output = content;
            event.structured_data = "maven_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, build_failure_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "maven";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "build_failure";
            event.message = "Maven build failed";
            event.raw_output = content;
            event.structured_data = "maven_build";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;
            events.push_back(event);
        }
        else if (std::regex_search(line, match, test_result_pattern)) {
            int total_tests = std::stoi(match[1].str());
            int failures = std::stoi(match[2].str());
            int errors = std::stoi(match[3].str());
            int skipped = std::stoi(match[4].str());

            if (total_tests > 0) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "maven-surefire";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
                event.severity = (failures > 0 || errors > 0) ? "error" : "info";
                event.category = "test_summary";
                event.message = "Tests: " + std::to_string(total_tests) +
                               " total, " + std::to_string(failures) + " failures, " +
                               std::to_string(errors) + " errors, " +
                               std::to_string(skipped) + " skipped";
                event.raw_output = content;
                event.structured_data = "maven_build";
                events.push_back(event);
            }
        }
    }

    return events;
}

} // namespace duckdb
