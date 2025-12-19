#include "python_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

bool PythonBuildParser::canParse(const std::string& content) const {
    // Check for Python build patterns
    return (content.find("ERROR: Failed building wheel for") != std::string::npos) ||
           (content.find("FAILED ") != std::string::npos && content.find("::") != std::string::npos) ||
           (content.find("ERROR ") != std::string::npos && content.find("::") != std::string::npos) ||
           (content.find("AssertionError:") != std::string::npos || content.find("TypeError:") != std::string::npos) ||
           (content.find("error: command") != std::string::npos && content.find("failed with exit status") != std::string::npos) ||
           (content.find("setuptools") != std::string::npos && content.find(".c:") != std::string::npos);
}

std::vector<ValidationEvent> PythonBuildParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;
    std::string current_test;

    // Pre-compiled regex patterns
    static const std::regex package_pattern(R"(ERROR: Failed building wheel for ([^\s,]+))");
    static const std::regex c_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*error:\s*(.+))");
    static const std::regex test_failed_pattern(R"(FAILED\s+([^:]+::[\w_]+))");
    static const std::regex test_error_pattern(R"(ERROR\s+([^:]+::[\w_]+))");
    static const std::regex location_pattern(R"(\s*([^:]+):(\d+):\s+in\s+(\w+))");
    static const std::regex cmd_pattern(R"(error: command '([^']+)')");
    static const std::regex c_warn_pattern(R"(([^:]+):(\d+):(\d*):?\s*warning:\s*(.+))");

    while (std::getline(stream, line)) {
        current_line_num++;

        // Parse pip wheel building errors
        if (line.find("ERROR: Failed building wheel for") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pip";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "package_build";
            event.severity = "error";
            event.message = line;
            event.ref_line = -1;
            event.ref_column = -1;
            event.log_content = content;
            event.structured_data = "python_build";

            // Extract package name
            std::smatch package_match;
            if (std::regex_search(line, package_match, package_pattern)) {
                event.test_name = package_match[1].str();
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse setuptools/distutils compilation errors (C extension errors)
        else if (line.find("error:") != std::string::npos &&
                (line.find(".c:") != std::string::npos || line.find(".cpp:") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "compilation";
            event.severity = "error";
            event.message = line;
            event.log_content = content;
            event.structured_data = "python_build";

            // Extract file and line info from C compilation errors
            std::smatch c_match;
            if (std::regex_search(line, c_match, c_error_pattern)) {
                event.ref_file = c_match[1].str();
                event.ref_line = std::stoi(c_match[2].str());
                event.ref_column = c_match[3].str().empty() ? -1 : std::stoi(c_match[3].str());
                event.message = c_match[4].str();
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse Python test failures (pytest format)
        else if (line.find("FAILED ") != std::string::npos && line.find("::") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.log_content = content;
            event.structured_data = "python_build";

            // Extract test name
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_failed_pattern)) {
                event.test_name = test_match[1].str();
                // Extract file path
                size_t sep_pos = event.test_name.find("::");
                if (sep_pos != std::string::npos) {
                    event.ref_file = event.test_name.substr(0, sep_pos);
                }
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse Python test errors
        else if (line.find("ERROR ") != std::string::npos && line.find("::") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::ERROR;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.log_content = content;
            event.structured_data = "python_build";

            // Extract test name
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_error_pattern)) {
                event.test_name = test_match[1].str();
                // Extract file path
                size_t sep_pos = event.test_name.find("::");
                if (sep_pos != std::string::npos) {
                    event.ref_file = event.test_name.substr(0, sep_pos);
                }
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse assertion errors with file:line info
        else if (line.find("AssertionError:") != std::string::npos || line.find("TypeError:") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = ValidationEventType::TEST_RESULT;
            event.status = ValidationEventStatus::FAIL;
            event.category = "assertion";
            event.severity = "error";
            event.message = line;
            event.log_content = content;
            event.structured_data = "python_build";

            // These are usually part of a test failure context
            if (!current_test.empty()) {
                event.test_name = current_test;
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse file:line test location info
        else if (std::regex_match(line, location_pattern)) {
            std::smatch loc_match;
            if (std::regex_search(line, loc_match, location_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "pytest";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = ValidationEventStatus::INFO;
                event.category = "traceback";
                event.severity = "info";
                event.ref_file = loc_match[1].str();
                event.ref_line = std::stoi(loc_match[2].str());
                event.function_name = loc_match[3].str();
                event.message = line;
                event.log_content = content;
                event.structured_data = "python_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse setup.py command failures
        else if (line.find("error: command") != std::string::npos && line.find("failed with exit status") != std::string::npos) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::ERROR;
            event.category = "build_command";
            event.severity = "error";
            event.message = line;
            event.ref_line = -1;
            event.ref_column = -1;
            event.log_content = content;
            event.structured_data = "python_build";

            // Extract command name
            std::smatch cmd_match;
            if (std::regex_search(line, cmd_match, cmd_pattern)) {
                event.function_name = cmd_match[1].str();
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse C extension warnings
        else if (line.find("warning:") != std::string::npos &&
                (line.find(".c:") != std::string::npos || line.find(".cpp:") != std::string::npos)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = ValidationEventType::BUILD_ERROR;
            event.status = ValidationEventStatus::WARNING;
            event.category = "compilation";
            event.severity = "warning";
            event.message = line;
            event.log_content = content;
            event.structured_data = "python_build";

            // Extract file and line info
            std::smatch c_match;
            if (std::regex_search(line, c_match, c_warn_pattern)) {
                event.ref_file = c_match[1].str();
                event.ref_line = std::stoi(c_match[2].str());
                event.ref_column = c_match[3].str().empty() ? -1 : std::stoi(c_match[3].str());
                event.message = c_match[4].str();
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
    }

    return events;
}

} // namespace duckdb
