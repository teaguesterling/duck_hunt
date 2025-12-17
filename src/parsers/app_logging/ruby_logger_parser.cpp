#include "ruby_logger_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Ruby Logger levels
static std::string MapRubyLevel(char level_char) {
    switch (level_char) {
        case 'F': // FATAL
        case 'E': // ERROR
            return "error";
        case 'W': // WARN
            return "warning";
        default:  // I (INFO), D (DEBUG), A (ANY/UNKNOWN)
            return "info";
    }
}

static ValidationEventStatus MapLevelToStatus(const std::string& severity) {
    if (severity == "error") return ValidationEventStatus::ERROR;
    if (severity == "warning") return ValidationEventStatus::WARNING;
    return ValidationEventStatus::INFO;
}

// Parse Ruby Logger format:
// "I, [2025-01-15T10:30:45.123456 #1234]  INFO -- myapp: User logged in"
// "E, [2025-01-15T10:30:46.456789 #1234] ERROR -- myapp: Connection failed"
static bool ParseRubyLoggerLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Ruby Logger format: L, [timestamp #pid] LEVEL -- progname: message
    static std::regex ruby_pattern(
        R"(^([FEWIDA]),\s+\[(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+)\s+#(\d+)\]\s+(\w+)\s+--\s+(\S+):\s*(.*)$)",
        std::regex::optimize
    );

    std::smatch match;
    if (!std::regex_match(line, match, ruby_pattern)) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "ruby_logger";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    char level_char = match[1].str()[0];
    event.started_at = match[2].str();
    std::string pid = match[3].str();
    std::string level_name = match[4].str();
    event.category = match[5].str();  // progname
    event.message = match[6].str();

    event.severity = MapRubyLevel(level_char);
    event.status = MapLevelToStatus(event.severity);

    // Build structured_data JSON
    std::string json = "{";
    json += "\"level\":\"" + level_name + "\"";
    json += ",\"pid\":\"" + pid + "\"";
    json += ",\"progname\":\"" + event.category + "\"";
    json += "}";
    event.structured_data = json;

    event.raw_output = line;
    return true;
}

bool RubyLoggerParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    std::istringstream stream(content);
    std::string line;
    int ruby_lines = 0;
    int checked = 0;

    // Ruby Logger detection: starts with "L, [timestamp #pid]"
    static std::regex ruby_detect(
        R"(^[FEWIDA],\s+\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+\s+#\d+\])",
        std::regex::optimize
    );

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, ruby_detect)) {
            ruby_lines++;
        }
    }

    return ruby_lines > 0 && ruby_lines >= (checked / 3);
}

std::vector<ValidationEvent> RubyLoggerParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int line_number = 0;

    while (std::getline(stream, line)) {
        line_number++;

        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
            line = line.substr(0, end + 1);
        }

        if (line.empty()) continue;

        ValidationEvent event;
        if (ParseRubyLoggerLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
