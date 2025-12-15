#include "pytest_parser.hpp"
#include "core/legacy_parser_registry.hpp"
#include <sstream>

namespace duckdb {

bool PytestParser::canParse(const std::string& content) const {
    // Check for pytest text patterns (file.py::test_name with PASSED/FAILED/SKIPPED)
    return content.find("::") != std::string::npos &&
           (content.find("PASSED") != std::string::npos ||
            content.find("FAILED") != std::string::npos ||
            content.find("SKIPPED") != std::string::npos);
}

std::vector<ValidationEvent> PytestParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;

    while (std::getline(stream, line)) {
        current_line_num++;
        if (line.empty()) continue;

        // Look for pytest text output patterns: "file.py::test_name STATUS"
        if (line.find("::") != std::string::npos) {
            parseTestLine(line, event_id, events, current_line_num);
        }
    }

    return events;
}

void PytestParser::parseTestLine(const std::string& line, int64_t& event_id,
                                std::vector<ValidationEvent>& events, int32_t log_line_num) const {
    // Parse pytest test result line using simple string parsing
    // Format 1: "file.py::test_name STATUS" or "file.py::test_name STATUS [extra]"
    // Format 2: "STATUS file.py::test_name - message" (short test summary)
    size_t separator = line.find("::");
    if (separator == std::string::npos) {
        return;
    }

    ValidationEvent event;
    event.event_id = event_id++;
    event.tool_name = "pytest";
    event.event_type = ValidationEventType::TEST_RESULT;
    event.line_number = -1;
    event.column_number = -1;
    event.execution_time = 0.0;
    event.category = "test";
    event.raw_output = line;
    event.structured_data = "pytest_text";
    event.log_line_start = log_line_num;
    event.log_line_end = log_line_num;

    // Check for Format 2: "STATUS file.py::test_name - message" (status at start)
    bool status_at_start = false;
    if (line.rfind("FAILED ", 0) == 0) {
        event.status = ValidationEventStatus::FAIL;
        event.severity = "error";
        status_at_start = true;
    } else if (line.rfind("PASSED ", 0) == 0) {
        event.status = ValidationEventStatus::PASS;
        event.severity = "info";
        status_at_start = true;
    } else if (line.rfind("SKIPPED ", 0) == 0) {
        event.status = ValidationEventStatus::SKIP;
        event.severity = "warning";
        status_at_start = true;
    } else if (line.rfind("ERROR ", 0) == 0) {
        event.status = ValidationEventStatus::ERROR;
        event.severity = "error";
        status_at_start = true;
    }

    if (status_at_start) {
        // Format 2: "STATUS file.py::test_name - message"
        // Find the file path (starts after "STATUS ")
        size_t file_start = line.find(' ') + 1;
        event.file_path = line.substr(file_start, separator - file_start);

        // Find the test name (between :: and " - " or end)
        std::string rest = line.substr(separator + 2);
        size_t dash_pos = rest.find(" - ");
        if (dash_pos != std::string::npos) {
            event.test_name = rest.substr(0, dash_pos);
            event.message = rest.substr(dash_pos + 3);  // Get the message after " - "
        } else {
            event.test_name = rest;
            event.message = "Test " + std::string(event.status == ValidationEventStatus::FAIL ? "failed" :
                           event.status == ValidationEventStatus::PASS ? "passed" :
                           event.status == ValidationEventStatus::SKIP ? "skipped" : "error");
        }
    } else {
        // Format 1: "file.py::test_name STATUS [extra]"
        event.file_path = line.substr(0, separator);
        std::string rest = line.substr(separator + 2);

        // Find the status at the end
        if (rest.find(" PASSED") != std::string::npos) {
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.message = "Test passed";
            event.test_name = rest.substr(0, rest.find(" PASSED"));
        } else if (rest.find(" FAILED") != std::string::npos) {
            event.status = ValidationEventStatus::FAIL;
            event.severity = "error";
            event.message = "Test failed";
            event.test_name = rest.substr(0, rest.find(" FAILED"));
        } else if (rest.find(" ERROR") != std::string::npos) {
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.message = "Test error";
            event.test_name = rest.substr(0, rest.find(" ERROR"));
        } else if (rest.find(" SKIPPED") != std::string::npos) {
            event.status = ValidationEventStatus::SKIP;
            event.severity = "warning";
            event.message = "Test skipped";
            event.test_name = rest.substr(0, rest.find(" SKIPPED"));
        } else {
            // No recognized status - skip this line
            event_id--;  // Undo the increment
            return;
        }
    }

    events.push_back(event);
}

// Auto-register this parser
REGISTER_PARSER(PytestParser);

} // namespace duckdb