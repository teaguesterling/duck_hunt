#include "duckdb_test_parser.hpp"
#include <sstream>
#include <string>

namespace duck_hunt {

bool DuckDBTestParser::CanParse(const std::string& content) const {
    // Check for DuckDB test output patterns
    return (content.find("unittest is a Catch") != std::string::npos ||
            content.find("test cases:") != std::string::npos ||
            (content.find("[") == 0 && content.find("]: test/") != std::string::npos) ||
            content.find("Wrong result in query!") != std::string::npos);
}

void DuckDBTestParser::Parse(const std::string& content, std::vector<duckdb::ValidationEvent>& events) const {
    ParseDuckDBTestOutput(content, events);
}

void DuckDBTestParser::ParseDuckDBTestOutput(const std::string& content, std::vector<duckdb::ValidationEvent>& events) {
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;

    // Track current test being processed
    std::string current_test_file;
    bool in_failure_section = false;
    std::string failure_message;
    std::string failure_query;
    int failure_line = -1;
    std::string mismatch_details;
    std::string expected_result;
    std::string actual_result;
    bool in_expected_section = false;
    bool in_actual_section = false;

    while (std::getline(stream, line)) {
        // Parse test progress lines: [X/Y] (Z%): /path/to/test.test
        if (line.find("[") == 0 && line.find("]:") != std::string::npos) {
            size_t path_start = line.find("): ");
            if (path_start != std::string::npos) {
                current_test_file = line.substr(path_start + 3);
                // Trim any trailing whitespace/dots
                while (!current_test_file.empty() &&
                       (current_test_file.back() == '.' || current_test_file.back() == ' ')) {
                    current_test_file.pop_back();
                }
            }
        }

        // Detect failure start
        else if (line.find("Wrong result in query!") != std::string::npos ||
                 line.find("Query unexpectedly failed") != std::string::npos) {
            in_failure_section = true;
            failure_message = line;

            // Extract line number from failure message
            size_t line_start = line.find(".test:");
            if (line_start != std::string::npos) {
                line_start += 6; // Length of ".test:"
                size_t line_end = line.find(")", line_start);
                if (line_end != std::string::npos) {
                    try {
                        failure_line = std::stoi(line.substr(line_start, line_end - line_start));
                    } catch (...) {
                        failure_line = -1;
                    }
                }
            }
        }

        // Capture SQL query in failure section
        else if (in_failure_section && !line.empty() &&
                 line.find("================================================================================") == std::string::npos &&
                 line.find("SELECT") == 0) {
            failure_query = line;
        }

        // Capture mismatch details
        else if (in_failure_section && line.find("Mismatch on row") != std::string::npos) {
            mismatch_details = line;
        }

        // Detect expected result section
        else if (in_failure_section && line.find("Expected result:") != std::string::npos) {
            in_expected_section = true;
            in_actual_section = false;
        }

        // Detect actual result section
        else if (in_failure_section && line.find("Actual result:") != std::string::npos) {
            in_expected_section = false;
            in_actual_section = true;
        }

        // Capture expected result data
        else if (in_expected_section && !line.empty() &&
                 line.find("================================================================================") == std::string::npos) {
            if (!expected_result.empty()) expected_result += "\n";
            expected_result += line;
        }

        // Capture actual result data
        else if (in_actual_section && !line.empty() &&
                 line.find("================================================================================") == std::string::npos) {
            if (!actual_result.empty()) actual_result += "\n";
            actual_result += line;
        }

        // End of failure section - create failure event
        else if (in_failure_section && line.find("FAILED:") != std::string::npos) {
            duckdb::ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "duckdb_test";
            event.event_type = duckdb::ValidationEventType::TEST_RESULT;
            event.ref_file = current_test_file;
            event.ref_line = failure_line;
            event.ref_column = -1;
            event.function_name = failure_query.empty() ? "unknown" : failure_query.substr(0, 50);
            event.status = duckdb::ValidationEventStatus::FAIL;
            event.category = "test_failure";

            // Enhanced message with mismatch details
            std::string enhanced_message = failure_message;
            if (!mismatch_details.empty()) {
                enhanced_message += " | " + mismatch_details;
            }
            event.message = enhanced_message;

            // Enhanced raw_output with query and comparison details
            std::string enhanced_output = failure_query;
            if (!expected_result.empty() && !actual_result.empty()) {
                enhanced_output += "\n--- Expected ---\n" + expected_result;
                enhanced_output += "\n--- Actual ---\n" + actual_result;
            }
            event.log_content = enhanced_output;

            // Use suggestion field for mismatch details
            event.suggestion = mismatch_details;
            event.execution_time = 0.0;

            events.push_back(event);

            // Reset failure tracking
            in_failure_section = false;
            in_expected_section = false;
            in_actual_section = false;
            failure_message.clear();
            failure_query.clear();
            failure_line = -1;
            mismatch_details.clear();
            expected_result.clear();
            actual_result.clear();
        }

        // Parse summary line: test cases: X | Y passed | Z failed
        else if (line.find("test cases:") != std::string::npos) {
            // Extract summary statistics and create summary events
            std::string summary_line = line;

            // Extract passed count
            size_t passed_pos = summary_line.find(" passed");
            if (passed_pos != std::string::npos) {
                size_t passed_start = summary_line.rfind(" ", passed_pos - 1);
                if (passed_start != std::string::npos) {
                    try {
                        int passed_count = std::stoi(summary_line.substr(passed_start + 1, passed_pos - passed_start - 1));

                        // Create passed test events (summary)
                        duckdb::ValidationEvent summary_event;
                        summary_event.event_id = event_id++;
                        summary_event.tool_name = "duckdb_test";
                        summary_event.event_type = duckdb::ValidationEventType::TEST_RESULT;
                        summary_event.status = duckdb::ValidationEventStatus::INFO;
                        summary_event.category = "test_summary";
                        summary_event.message = "Test summary: " + std::to_string(passed_count) + " tests passed";
                        summary_event.ref_line = -1;
                        summary_event.ref_column = -1;
                        summary_event.execution_time = 0.0;

                        events.push_back(summary_event);
                    } catch (...) {
                        // Ignore parsing errors
                    }
                }
            }
        }
    }

    // If no events were created, add a basic summary
    if (events.empty()) {
        duckdb::ValidationEvent summary_event;
        summary_event.event_id = 1;
        summary_event.tool_name = "duckdb_test";
        summary_event.event_type = duckdb::ValidationEventType::TEST_RESULT;
        summary_event.status = duckdb::ValidationEventStatus::INFO;
        summary_event.category = "test_summary";
        summary_event.message = "DuckDB test output parsed (no specific test results found)";
        summary_event.ref_line = -1;
        summary_event.ref_column = -1;
        summary_event.execution_time = 0.0;

        events.push_back(summary_event);
    }
}

} // namespace duck_hunt
