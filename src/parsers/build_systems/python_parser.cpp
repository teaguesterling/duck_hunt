#include "python_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool PythonParser::CanParse(const std::string& content) const {
    // Check for Python build patterns
    return (content.find("ERROR: Failed building wheel for") != std::string::npos) ||
           (content.find("FAILED ") != std::string::npos && content.find("::") != std::string::npos) ||
           (content.find("ERROR ") != std::string::npos && content.find("::") != std::string::npos) ||
           (content.find("AssertionError:") != std::string::npos || content.find("TypeError:") != std::string::npos) ||
           (content.find("error: command") != std::string::npos && content.find("failed with exit status") != std::string::npos) ||
           (content.find("setuptools") != std::string::npos && content.find(".c:") != std::string::npos);
}

void PythonParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParsePythonBuild(content, events);
}

void PythonParser::ParsePythonBuild(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;
    std::string current_test;
    std::string current_package;

    while (std::getline(stream, line)) {
        current_line_num++;
        // Parse pip wheel building errors
        if (line.find("ERROR: Failed building wheel for") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pip";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "package_build";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract package name
            std::regex package_pattern(R"(ERROR: Failed building wheel for ([^\s,]+))");
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "compilation";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract file and line info from C compilation errors
            std::regex c_error_pattern(R"(([^:]+):(\d+):(\d*):?\s*error:\s*(.+))");
            std::smatch c_match;
            if (std::regex_search(line, c_match, c_error_pattern)) {
                event.file_path = c_match[1].str();
                event.line_number = std::stoi(c_match[2].str());
                event.column_number = c_match[3].str().empty() ? -1 : std::stoi(c_match[3].str());
                event.message = c_match[4].str();
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse Python test failures (pytest format)
        else if (line.find("FAILED ") != std::string::npos && line.find("::") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract test name
            std::regex test_pattern(R"(FAILED\s+([^:]+::[\w_]+))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_pattern)) {
                event.test_name = test_match[1].str();
                // Extract file path
                size_t sep_pos = event.test_name.find("::");
                if (sep_pos != std::string::npos) {
                    event.file_path = event.test_name.substr(0, sep_pos);
                }
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse Python test errors
        else if (line.find("ERROR ") != std::string::npos && line.find("::") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract test name
            std::regex test_pattern(R"(ERROR\s+([^:]+::[\w_]+))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_pattern)) {
                event.test_name = test_match[1].str();
                // Extract file path
                size_t sep_pos = event.test_name.find("::");
                if (sep_pos != std::string::npos) {
                    event.file_path = event.test_name.substr(0, sep_pos);
                }
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
        // Parse assertion errors with file:line info
        else if (line.find("AssertionError:") != std::string::npos || line.find("TypeError:") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "pytest";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.category = "assertion";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
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
        else if (std::regex_match(line, std::regex(R"(\s*([^:]+):(\d+):\s+in\s+\w+)"))) {
            std::regex location_pattern(R"(\s*([^:]+):(\d+):\s+in\s+(\w+))");
            std::smatch loc_match;
            if (std::regex_search(line, loc_match, location_pattern)) {
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "pytest";
                event.event_type = duckdb::ValidationEventType::TEST_RESULT;
                event.status = duckdb::ValidationEventStatus::INFO;
                event.category = "traceback";
                event.severity = "info";
                event.file_path = loc_match[1].str();
                event.line_number = std::stoi(loc_match[2].str());
                event.function_name = loc_match[3].str();
                event.message = line;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "python_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse setup.py command failures
        else if (line.find("error: command") != std::string::npos && line.find("failed with exit status") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "build_command";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract command name
            std::regex cmd_pattern(R"(error: command '([^']+)')");
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
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "setuptools";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::WARNING;
            event.category = "compilation";
            event.severity = "warning";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "python_build";
            
            // Extract file and line info
            std::regex c_warn_pattern(R"(([^:]+):(\d+):(\d*):?\s*warning:\s*(.+))");
            std::smatch c_match;
            if (std::regex_search(line, c_match, c_warn_pattern)) {
                event.file_path = c_match[1].str();
                event.line_number = std::stoi(c_match[2].str());
                event.column_number = c_match[3].str().empty() ? -1 : std::stoi(c_match[3].str());
                event.message = c_match[4].str();
            }
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
        }
    }
}

} // namespace duck_hunt