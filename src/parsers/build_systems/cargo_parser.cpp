#include "cargo_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

bool CargoParser::canParse(const std::string& content) const {
    // Check for Cargo/Rust patterns
    return (content.find("error[E") != std::string::npos && content.find("-->") != std::string::npos) ||
           (content.find("warning:") != std::string::npos && content.find("-->") != std::string::npos) ||
           (content.find("test ") != std::string::npos && content.find("FAILED") != std::string::npos) ||
           (content.find("thread '") != std::string::npos && content.find("panicked at") != std::string::npos) ||
           (content.find("error: could not compile") != std::string::npos) ||
           (content.find("clippy::") != std::string::npos) ||
           (content.find("Diff in") != std::string::npos && content.find("at line") != std::string::npos);
}

std::vector<ValidationEvent> CargoParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    // Pre-compiled regex patterns
    static const std::regex rust_error_pattern(R"(error\[E(\d+)\]:\s*(.+))");
    static const std::regex warning_pattern(R"(warning:\s*(.+))");
    static const std::regex location_pattern(R"(-->\s*([^:]+):(\d+):(\d+))");
    static const std::regex test_pattern(R"(test\s+([^\s]+)\s+\.\.\.\s+FAILED)");
    static const std::regex panic_pattern(R"(thread '([^']+)' panicked at '([^']+)',\s*([^:]+):(\d+):(\d+))");
    static const std::regex clippy_pattern(R"(([^:]+):(\d+):(\d+):\s*(warning|error):\s*(.+))");
    static const std::regex compile_error_pattern(R"(error: could not compile `([^`]+)`)");
    static const std::regex summary_pattern(R"(test result: FAILED\.\s*(\d+) passed;\s*(\d+) failed)");
    static const std::regex fmt_pattern(R"(Diff in ([^\s]+) at line (\d+):)");

    while (std::getline(stream, line)) {
        current_line_num++;
        std::smatch match;

        // Parse Rust compiler errors: error[E0XXX]: message --> file:line:column
        if (std::regex_search(line, match, rust_error_pattern)) {
            std::string error_code = "E" + match[1].str();
            std::string message = match[2].str();

            // Look ahead for the file location line
            std::string location_line;
            int32_t start_line = current_line_num;
            if (std::getline(stream, location_line) && location_line.find("-->") != std::string::npos) {
                current_line_num++;
                std::smatch loc_match;

                if (std::regex_search(location_line, loc_match, location_pattern)) {
                    ValidationEvent event;
                    event.event_id = event_id++;
                    event.tool_name = "rustc";
                    event.event_type = ValidationEventType::BUILD_ERROR;
                    event.ref_file = loc_match[1].str();
                    event.ref_line = std::stoi(loc_match[2].str());
                    event.ref_column = std::stoi(loc_match[3].str());
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                    event.category = "compilation";
                    event.message = message;
                    event.error_code = error_code;
                    event.log_content = content;
                    event.structured_data = "cargo_build";
                    event.log_line_start = start_line;
                    event.log_line_end = current_line_num;

                    events.push_back(event);
                }
            }
        }
        // Parse warning patterns: warning: message --> file:line:column
        else if (line.find("warning:") != std::string::npos && line.find("clippy::") == std::string::npos) {
            std::smatch warn_match;

            if (std::regex_search(line, warn_match, warning_pattern)) {
                std::string message = warn_match[1].str();

                // Look ahead for the file location line
                std::string location_line;
                int32_t start_line = current_line_num;
                if (std::getline(stream, location_line) && location_line.find("-->") != std::string::npos) {
                    current_line_num++;
                    std::smatch loc_match;

                    if (std::regex_search(location_line, loc_match, location_pattern)) {
                        ValidationEvent event;
                        event.event_id = event_id++;
                        event.tool_name = "rustc";
                        event.event_type = ValidationEventType::LINT_ISSUE;
                        event.ref_file = loc_match[1].str();
                        event.ref_line = std::stoi(loc_match[2].str());
                        event.ref_column = std::stoi(loc_match[3].str());
                        event.status = ValidationEventStatus::WARNING;
                        event.severity = "warning";
                        event.category = "compilation";
                        event.message = message;
                        event.log_content = content;
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
            std::smatch test_match;

            if (std::regex_search(line, test_match, test_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = test_match[1].str();
                event.function_name = test_match[1].str();
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_failure";
                event.message = "Test failed";
                event.ref_line = -1;
                event.ref_column = -1;
                event.log_content = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo test panic details: thread 'test_name' panicked at 'message', file:line:column
        else if (line.find("thread '") != std::string::npos && line.find("panicked at") != std::string::npos) {
            std::smatch panic_match;

            if (std::regex_search(line, panic_match, panic_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.test_name = panic_match[1].str();
                event.function_name = panic_match[1].str();
                event.ref_file = panic_match[3].str();
                event.ref_line = std::stoi(panic_match[4].str());
                event.ref_column = std::stoi(panic_match[5].str());
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_panic";
                event.message = panic_match[2].str();
                event.log_content = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo clippy warnings and errors
        else if ((line.find("clippy::") != std::string::npos || line.find("warning:") != std::string::npos) &&
                 (line.find("-->") != std::string::npos || line.find("src/") != std::string::npos)) {
            std::smatch clippy_match;

            if (std::regex_search(line, clippy_match, clippy_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "clippy";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.ref_file = clippy_match[1].str();
                event.ref_line = std::stoi(clippy_match[2].str());
                event.ref_column = std::stoi(clippy_match[3].str());

                std::string severity = clippy_match[4].str();
                if (severity == "error") {
                    event.status = ValidationEventStatus::ERROR;
                    event.severity = "error";
                    event.category = "lint_error";
                } else {
                    event.status = ValidationEventStatus::WARNING;
                    event.severity = "warning";
                    event.category = "lint_warning";
                }

                event.message = clippy_match[5].str();
                event.log_content = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo build/compilation failures
        else if (line.find("error: could not compile") != std::string::npos) {
            std::smatch compile_match;

            if (std::regex_search(line, compile_match, compile_error_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::BUILD_ERROR;
                event.function_name = compile_match[1].str();
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "compilation";
                event.message = "Could not compile package: " + compile_match[1].str();
                event.ref_line = -1;
                event.ref_column = -1;
                event.log_content = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo test result summary: test result: FAILED. X passed; Y failed; Z ignored
        else if (line.find("test result: FAILED") != std::string::npos) {
            std::smatch summary_match;

            if (std::regex_search(line, summary_match, summary_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "cargo";
                event.event_type = ValidationEventType::TEST_RESULT;
                event.status = ValidationEventStatus::ERROR;
                event.severity = "error";
                event.category = "test_summary";
                event.message = "Test suite failed: " + summary_match[2].str() + " failed, " +
                               summary_match[1].str() + " passed";
                event.ref_line = -1;
                event.ref_column = -1;
                event.log_content = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
        // Parse cargo fmt check differences
        else if (line.find("Diff in") != std::string::npos && line.find("at line") != std::string::npos) {
            std::smatch fmt_match;

            if (std::regex_search(line, fmt_match, fmt_pattern)) {
                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "rustfmt";
                event.event_type = ValidationEventType::LINT_ISSUE;
                event.ref_file = fmt_match[1].str();
                event.ref_line = std::stoi(fmt_match[2].str());
                event.ref_column = -1;
                event.status = ValidationEventStatus::WARNING;
                event.severity = "warning";
                event.category = "formatting";
                event.message = "Code formatting difference detected";
                event.log_content = content;
                event.structured_data = "cargo_build";
                event.log_line_start = current_line_num;
                event.log_line_end = current_line_num;

                events.push_back(event);
            }
        }
    }

    return events;
}

} // namespace duckdb
