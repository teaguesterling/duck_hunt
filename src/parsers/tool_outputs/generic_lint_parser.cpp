#include "generic_lint_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

bool GenericLintParser::canParse(const std::string& content) const {
    // Check for generic lint format: file:line:column: level: message
    std::regex lint_pattern(R"([^:]+:\d+:\d*:?\s*(error|warning|info|note):\s*.+)");
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (std::regex_search(line, lint_pattern)) {
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
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;

    while (std::getline(stream, line)) {
        // Parse generic lint format: file:line:column: level: message
        std::regex lint_pattern(R"(([^:]+):(\d+):(\d*):?\s*(error|warning|info|note):\s*(.+))");
        std::smatch match;

        if (std::regex_match(line, match, lint_pattern)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "lint";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.ref_file = match[1].str();
            event.ref_line = std::stoi(match[2].str());
            event.ref_column = match[3].str().empty() ? -1 : std::stoi(match[3].str());
            event.function_name = "";
            event.message = match[5].str();
            event.execution_time = 0.0;

            std::string level = match[4].str();
            if (level == "error") {
                event.status = ValidationEventStatus::ERROR;
                event.category = "lint_error";
                event.severity = "error";
            } else if (level == "warning") {
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
