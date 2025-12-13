#include "cargo_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duck_hunt {

bool CargoParser::CanParse(const std::string& content) const {
    // Check for Cargo/Rust patterns
    return (content.find("error[E") != std::string::npos && content.find("-->") != std::string::npos) ||
           (content.find("warning:") != std::string::npos && content.find("-->") != std::string::npos) ||
           (content.find("test ") != std::string::npos && content.find("FAILED") != std::string::npos) ||
           (content.find("thread '") != std::string::npos && content.find("panicked at") != std::string::npos) ||
           (content.find("error: could not compile") != std::string::npos) ||
           (content.find("clippy::") != std::string::npos) ||
           (content.find("Diff in") != std::string::npos && content.find("at line") != std::string::npos);
}

void CargoParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseCargoBuild(content, events);
}

void CargoParser::ParseCargoBuild(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    while (std::getline(stream, line)) {
        current_line_num++;
        // Parse Rust compiler errors: error[E0XXX]: message --> file:line:column
        std::regex rust_error_pattern(R"(error\[E(\d+)\]:\s*(.+))");
        std::smatch match;
        
        if (std::regex_search(line, match, rust_error_pattern)) {
            std::string error_code = "E" + match[1].str();
            std::string message = match[2].str();
            
            // Look ahead for the file location line
            std::string location_line;
            int32_t start_line = current_line_num;
            if (std::getline(stream, location_line) && location_line.find("-->") != std::string::npos) {
                current_line_num++;  // Account for the lookahead line
                std::regex location_pattern(R"(-->\s*([^:]+):(\d+):(\d+))");
                std::smatch loc_match;

                if (std::regex_search(location_line, loc_match, location_pattern)) {
                    duckdb::ValidationEvent event;
                    event.event_id = event_id++;
                    event.tool_name = "rustc";
                    event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
                    event.file_path = loc_match[1].str();
                    event.line_number = std::stoi(loc_match[2].str());
                    event.column_number = std::stoi(loc_match[3].str());
                    event.function_name = "";
                    event.status = duckdb::ValidationEventStatus::ERROR;
                    event.severity = "error";
                    event.category = "compilation";
                    event.message = message;
                    event.error_code = error_code;
                    event.execution_time = 0.0;
                    event.raw_output = content;
                    event.structured_data = "cargo_build";
                    event.log_line_start = start_line;
                    event.log_line_end = current_line_num;

                    events.push_back(event);
                }
            }
        }
        // Parse warning patterns: warning: message --> file:line:column
        else if (line.find("warning:") != std::string::npos) {
            std::regex warning_pattern(R"(warning:\s*(.+))");
            std::smatch warn_match;
            
            if (std::regex_search(line, warn_match, warning_pattern)) {
                std::string message = warn_match[1].str();
                
                // Look ahead for the file location line
                std::string location_line;
                int32_t start_line = current_line_num;
                if (std::getline(stream, location_line) && location_line.find("-->") != std::string::npos) {
                    current_line_num++;  // Account for the lookahead line
                    std::regex location_pattern(R"(-->\s*([^:]+):(\d+):(\d+))");
                    std::smatch loc_match;

                    if (std::regex_search(location_line, loc_match, location_pattern)) {
                        duckdb::ValidationEvent event;
                        event.event_id = event_id++;
                        event.tool_name = "rustc";
                        event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
                        event.file_path = loc_match[1].str();
                        event.line_number = std::stoi(loc_match[2].str());
                        event.column_number = std::stoi(loc_match[3].str());
                        event.function_name = "";
                        event.status = duckdb::ValidationEventStatus::WARNING;
                        event.severity = "warning";
                        event.category = "compilation";
                        event.message = message;
                        event.execution_time = 0.0;
                        event.raw_output = content;
                        event.structured_data = "cargo_build";
                        event.log_line_start = start_line;
                        event.log_line_end = current_line_num;

                        events.push_back(event);
                    }
                }
            }
        }
        // Parse cargo test failures: test tests::test_name ... FAILED
        else if (line.find("test ") != std::string::npos && line.find("FAILED") != std::string::npos) {
            std::regex test_pattern(R"(test\s+([^\s]+)\s+\.\.\.\s+FAILED)");
            std::smatch test_match;
            
            if (std::regex_search(line, test_match, test_pattern)) {
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = duckdb::ValidationEventType::TEST_RESULT;
                event.test_name = test_match[1].str();
                event.function_name = test_match[1].str();
                event.status = duckdb::ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo test panic details: thread 'test_name' panicked at 'message', file:line:column
        else if (line.find("thread '") != std::string::npos && line.find("panicked at") != std::string::npos) {
            std::regex panic_pattern(R"(thread '([^']+)' panicked at '([^']+)',\s*([^:]+):(\d+):(\d+))");
            std::smatch panic_match;
            
            if (std::regex_search(line, panic_match, panic_pattern)) {
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = duckdb::ValidationEventType::TEST_RESULT;
                event.test_name = panic_match[1].str();
                event.function_name = panic_match[1].str();
                event.file_path = panic_match[3].str();
                event.line_number = std::stoi(panic_match[4].str());
                event.column_number = std::stoi(panic_match[5].str());
                event.status = duckdb::ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_panic";
                event.message = panic_match[2].str();
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo clippy warnings and errors
        else if ((line.find("clippy::") != std::string::npos || line.find("warning:") != std::string::npos) &&
                 (line.find("-->") != std::string::npos || line.find("src/") != std::string::npos)) {
            std::regex clippy_pattern(R"(([^:]+):(\d+):(\d+):\s*(warning|error):\s*(.+))");
            std::smatch clippy_match;
            
            if (std::regex_search(line, clippy_match, clippy_pattern)) {
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "clippy";
                event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
                event.file_path = clippy_match[1].str();
                event.line_number = std::stoi(clippy_match[2].str());
                event.column_number = std::stoi(clippy_match[3].str());
                event.function_name = "";
                
                std::string severity = clippy_match[4].str();
                if (severity == "error") {
                    event.status = duckdb::ValidationEventStatus::ERROR;
                    event.severity = "error";
                    event.category = "lint_error";
                } else {
                    event.status = duckdb::ValidationEventStatus::WARNING;
                    event.severity = "warning";
                    event.category = "lint_warning";
                }
                
                event.message = clippy_match[5].str();
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo build/compilation failures
        else if (line.find("error: could not compile") != std::string::npos) {
            std::regex compile_error_pattern(R"(error: could not compile `([^`]+)`)");
            std::smatch compile_match;
            
            if (std::regex_search(line, compile_match, compile_error_pattern)) {
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = duckdb::ValidationEventType::BUILD_ERROR;
                event.function_name = compile_match[1].str();
                event.status = duckdb::ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "compilation";
                event.message = "Could not compile package: " + compile_match[1].str();
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo test result summary: test result: FAILED. X passed; Y failed; Z ignored
        else if (line.find("test result: FAILED") != std::string::npos) {
            std::regex summary_pattern(R"(test result: FAILED\.\s*(\d+) passed;\s*(\d+) failed)");
            std::smatch summary_match;
            
            if (std::regex_search(line, summary_match, summary_pattern)) {
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = duckdb::ValidationEventType::TEST_RESULT;
                event.status = duckdb::ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_summary";
                event.message = "Test suite failed: " + summary_match[2].str() + " failed, " + 
                               summary_match[1].str() + " passed";
                event.line_number = -1;
                event.column_number = -1;
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo fmt check differences
        else if (line.find("Diff in") != std::string::npos && line.find("at line") != std::string::npos) {
            std::regex fmt_pattern(R"(Diff in ([^\s]+) at line (\d+):)");
            std::smatch fmt_match;
            
            if (std::regex_search(line, fmt_match, fmt_pattern)) {
                duckdb::ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "rustfmt";
                event.event_type = duckdb::ValidationEventType::LINT_ISSUE;
                event.file_path = fmt_match[1].str();
                event.line_number = std::stoi(fmt_match[2].str());
                event.column_number = -1;
                event.function_name = "";
                event.status = duckdb::ValidationEventStatus::WARNING;
                event.severity = "warning";
                event.category = "formatting";
                event.message = "Code formatting difference detected";
                event.execution_time = 0.0;
                event.raw_output = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
    }
}

} // namespace duck_hunt