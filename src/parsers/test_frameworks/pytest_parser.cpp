#include "pytest_parser.hpp"
#include "../../core/parser_registry.hpp"
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
    // Format: "file.py::test_name STATUS" or "file.py::test_name STATUS [extra]"
    size_t separator = line.find("::");
    if (separator == std::string::npos) {
        return;
    }

    ValidationEvent event;
    event.event_id = event_id++;
    event.tool_name = "pytest";
    event.event_type = ValidationEventType::TEST_RESULT;
    event.file_path = line.substr(0, separator);
    event.line_number = -1;
    event.column_number = -1;
    event.execution_time = 0.0;
    event.category = "test";
    event.raw_output = line;
    event.structured_data = "pytest_text";
    event.log_line_start = log_line_num;
    event.log_line_end = log_line_num;

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
        event.status = ValidationEventStatus::FAIL;
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
        return;
    }

    events.push_back(event);
}

// Auto-register this parser
REGISTER_PARSER(PytestParser);

} // namespace duckdb