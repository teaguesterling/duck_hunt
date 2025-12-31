#include "generic_lint_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>
#include <string>

namespace duckdb {

bool GenericLintParser::canParse(const std::string& content) const {
    // Check for generic lint format using safe string parsing (no regex backtracking risk)
    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int lines_checked = 0;

    while (reader.getLine(line) && lines_checked < 50) {
        lines_checked++;
        std::string file, severity, message;
        int line_num, col;
        if (SafeParsing::ParseCompilerDiagnostic(line, file, line_num, col, severity, message)) {
            return true;
        }
    }
    return false;
}

std::vector<ValidationEvent> GenericLintParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    ParseGenericLint(content, events);
    return events;
}

void GenericLintParser::ParseGenericLint(const std::string& content, std::vector<ValidationEvent>& events) {
    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int64_t event_id = 1;

    while (reader.getLine(line)) {
        // Parse generic lint format using safe string parsing (no regex backtracking risk)
        std::string file, severity, message;
        int line_num, col;

        if (SafeParsing::ParseCompilerDiagnostic(line, file, line_num, col, severity, message)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "lint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.ref_file = file;
            event.ref_line = line_num;
            event.ref_column = col;
            event.function_name = "";
            event.message = message;
            event.execution_time = 0.0;
            event.log_content = line;
            event.log_line_start = reader.lineNumber();
            event.log_line_end = reader.lineNumber();

            if (severity == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "lint_error";
                event.severity = "error";
            } else if (severity == "warning") {
                event.status = ValidationEventStatus::WARNING;
                event.category = "lint_warning";
                event.severity = "warning";
            } else {
                event.status = ValidationEventStatus::INFO;
                event.category = "lint_info";
                event.severity = "info";
            }

            events.push_back(event);
        }
    }

    // If no events were created, add a basic summary
    if (events.empty()) {
        ValidationEvent summary_event;
        summary_event.event_id = 1;
        summary_event.tool_name = "lint";
        summary_event.event_type = ValidationEventType::LINT_ISSUE;
        summary_event.status = ValidationEventStatus::INFO;
        summary_event.category = "lint_summary";
        summary_event.message = "Generic lint output parsed (no issues found)";
        summary_event.ref_line = -1;
        summary_event.ref_column = -1;
        summary_event.execution_time = 0.0;

        events.push_back(summary_event);
    }
}

} // namespace duckdb
