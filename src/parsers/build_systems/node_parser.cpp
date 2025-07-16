#include "node_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool NodeParser::CanParse(const std::string& content) const {
    // Check for Node.js/npm/yarn patterns
    return (content.find("npm ERR!") != std::string::npos || content.find("yarn install") != std::string::npos) ||
           (content.find("FAIL ") != std::string::npos && content.find(".test.js") != std::string::npos) ||
           (content.find("ERROR in") != std::string::npos && content.find("webpack") != std::string::npos) ||
           (content.find("● Test suite failed to run") != std::string::npos) ||
           (content.find("Could not resolve dependency:") != std::string::npos) ||
           (content.find("Module not found:") != std::string::npos);
}

void NodeParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseNodeBuild(content, events);
}

void NodeParser::ParseNodeBuild(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    std::string current_test_file;
    
    while (std::getline(stream, line)) {
        // Parse npm/yarn errors
        if (line.find("npm ERR!") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "npm";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "package_manager";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract error codes
            if (line.find("npm ERR! code") != std::string::npos) {
                std::regex code_pattern(R"(npm ERR! code ([A-Z_]+))");
                std::smatch code_match;
                if (std::regex_search(line, code_match, code_pattern)) {
                    event.error_code = code_match[1].str();
                }
            }
            
            events.push_back(event);
        }
        // Parse yarn errors  
        else if (line.find("error ") != std::string::npos && line.find("yarn") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "yarn";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "package_manager";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Jest test failures
        else if (line.find("FAIL ") != std::string::npos && line.find(".test.js") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.category = "test";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract test file
            std::regex test_file_pattern(R"(FAIL\s+([^\s]+\.test\.js))");
            std::smatch test_match;
            if (std::regex_search(line, test_match, test_file_pattern)) {
                event.file_path = test_match[1].str();
                current_test_file = event.file_path;
            }
            
            events.push_back(event);
        }
        // Parse Jest test suite failures
        else if (line.find("● Test suite failed to run") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "test_suite";
            event.severity = "error";
            event.message = line;
            event.file_path = current_test_file;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Jest individual test failures
        else if (line.find("●") != std::string::npos && line.find("›") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "jest";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.category = "test_case";
            event.severity = "error";
            event.message = line;
            event.file_path = current_test_file;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract test name
            std::regex test_name_pattern(R"(●\s+([^›]+)\s+›\s+(.+))");
            std::smatch name_match;
            if (std::regex_search(line, name_match, test_name_pattern)) {
                event.test_name = name_match[1].str() + " › " + name_match[2].str();
            }
            
            events.push_back(event);
        }
        // Parse ESLint errors and warnings
        else if (std::regex_match(line, std::regex(R"(\s*\d+:\d+\s+(error|warning)\s+.+)"))) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "eslint";
            event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Parse ESLint format: "  15:5   error    'console' is not defined    no-undef"
            std::regex eslint_pattern(R"(\s*(\d+):(\d+)\s+(error|warning)\s+(.+?)\s+([^\s]+)$)");
            std::smatch eslint_match;
            if (std::regex_search(line, eslint_match, eslint_pattern)) {
                event.line_number = std::stoi(eslint_match[1].str());
                event.column_number = std::stoi(eslint_match[2].str());
                std::string severity = eslint_match[3].str();
                event.message = eslint_match[4].str();
                event.error_code = eslint_match[5].str();
                
                if (severity == "error") {
                    event.status = duckdb::ValidationEventStatus::ERROR;
                    event.category = "lint_error";
                    event.severity = "error";
                } else {
                    event.status = duckdb::ValidationEventStatus::WARNING;
                    event.category = "lint_warning";
                    event.severity = "warning";
                }
            }
            
            events.push_back(event);
        }
        // Parse file paths for ESLint
        else if (line.find("/") != std::string::npos && line.find(".js") != std::string::npos && 
                line.find("  ") != 0 && line.find("error") == std::string::npos) {
            // This is likely a file path line like "/home/user/myproject/src/index.js"
            // Store it for the next ESLint errors
            if (!events.empty() && events.back().tool_name == "eslint" && events.back().file_path.empty()) {
                events.back().file_path = line;
            }
        }
        // Parse Webpack errors
        else if (line.find("ERROR in") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "webpack";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "bundling";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file and line info: "ERROR in ./src/components/Header.js"
            std::regex webpack_error_pattern(R"(ERROR in (.+?)(?:\s+(\d+):(\d+))?$)");
            std::smatch webpack_match;
            if (std::regex_search(line, webpack_match, webpack_error_pattern)) {
                event.file_path = webpack_match[1].str();
                if (webpack_match[2].matched) {
                    event.line_number = std::stoi(webpack_match[2].str());
                    event.column_number = std::stoi(webpack_match[3].str());
                }
            }
            
            events.push_back(event);
        }
        // Parse Webpack warnings
        else if (line.find("WARNING in") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "webpack";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::WARNING;
            event.category = "bundling";
            event.severity = "warning";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file info
            std::regex webpack_warn_pattern(R"(WARNING in (.+))");
            std::smatch webpack_match;
            if (std::regex_search(line, webpack_match, webpack_warn_pattern)) {
                event.file_path = webpack_match[1].str();
            }
            
            events.push_back(event);
        }
        // Parse syntax errors in compilation
        else if (line.find("Syntax error:") != std::string::npos || line.find("Parsing error:") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "javascript";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "syntax";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
        // Parse Node.js runtime errors with line numbers
        else if (line.find("at Object.<anonymous>") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "node";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "runtime";
            event.severity = "error";
            event.message = line;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            // Extract file and line info: "at Object.<anonymous> (src/index.test.js:5:23)"
            std::regex runtime_pattern(R"(at Object\.<anonymous> \(([^:]+):(\d+):(\d+)\))");
            std::smatch runtime_match;
            if (std::regex_search(line, runtime_match, runtime_pattern)) {
                event.file_path = runtime_match[1].str();
                event.line_number = std::stoi(runtime_match[2].str());
                event.column_number = std::stoi(runtime_match[3].str());
            }
            
            events.push_back(event);
        }
        // Parse dependency resolution errors
        else if (line.find("Could not resolve dependency:") != std::string::npos || 
                line.find("Module not found:") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "npm";
            event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
            event.status = duckdb::ValidationEventStatus::ERROR;
            event.category = "dependency";
            event.severity = "error";
            event.message = line;
            event.line_number = -1;
            event.column_number = -1;
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "node_build";
            
            events.push_back(event);
        }
    }
}

} // namespace duck_hunt