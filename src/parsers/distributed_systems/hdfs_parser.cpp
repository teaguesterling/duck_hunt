#include "hdfs_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

static std::string MapHdfsLevel(const std::string& level) {
    if (level == "ERROR" || level == "FATAL" || level == "SEVERE") {
        return "error";
    } else if (level == "WARN" || level == "WARNING") {
        return "warning";
    }
    return "info";
}

static ValidationEventStatus MapLevelToStatus(const std::string& severity) {
    if (severity == "error") {
        return ValidationEventStatus::ERROR;
    } else if (severity == "warning") {
        return ValidationEventStatus::WARNING;
    }
    return ValidationEventStatus::INFO;
}

// Parse HDFS log line using string operations (no regex to avoid backtracking)
// Format: YYMMDD HHMMSS pid LEVEL component: message
static bool ParseHdfsLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    if (line.size() < 25) return false;

    // Check date format: YYMMDD (6 digits)
    size_t pos = 0;
    for (int i = 0; i < 6; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }
    std::string date = line.substr(0, 6);

    // Skip space
    if (pos >= line.size() || line[pos] != ' ') return false;
    pos++;

    // Time: HHMMSS (6 digits)
    size_t time_start = pos;
    for (int i = 0; i < 6; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }
    std::string time = line.substr(time_start, 6);

    // Skip space
    if (pos >= line.size() || line[pos] != ' ') return false;
    pos++;

    // PID: digits
    size_t pid_start = pos;
    while (pos < line.size() && std::isdigit(line[pos])) {
        pos++;
    }
    if (pos == pid_start) return false;
    std::string pid = line.substr(pid_start, pos - pid_start);

    // Skip space
    if (pos >= line.size() || line[pos] != ' ') return false;
    pos++;

    // Level: alphabetic word
    size_t level_start = pos;
    while (pos < line.size() && std::isalpha(line[pos])) {
        pos++;
    }
    if (pos == level_start) return false;
    std::string level = line.substr(level_start, pos - level_start);

    // Validate level
    if (level != "INFO" && level != "WARN" && level != "WARNING" &&
        level != "ERROR" && level != "FATAL" && level != "DEBUG" && level != "TRACE") {
        return false;
    }

    // Skip space
    if (pos >= line.size() || line[pos] != ' ') return false;
    pos++;

    // Component: until ':'
    size_t comp_start = pos;
    while (pos < line.size() && line[pos] != ':') {
        pos++;
    }
    if (pos == comp_start || pos >= line.size()) return false;
    std::string component = line.substr(comp_start, pos - comp_start);

    // Skip ':'
    pos++;

    // Skip optional space after colon
    if (pos < line.size() && line[pos] == ' ') {
        pos++;
    }

    // Message: rest of line
    std::string message = (pos < line.size()) ? line.substr(pos) : "";

    // Build timestamp: 20YY-MM-DD HH:MM:SS
    std::string timestamp = "20" + date.substr(0, 2) + "-" + date.substr(2, 2) + "-" + date.substr(4, 2) +
                           " " + time.substr(0, 2) + ":" + time.substr(2, 2) + ":" + time.substr(4, 2);

    event.event_id = event_id;
    event.tool_name = "hdfs";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.ref_line = -1;
    event.ref_column = -1;

    event.started_at = timestamp;
    event.category = component;
    event.message = message;
    event.severity = MapHdfsLevel(level);
    event.status = MapLevelToStatus(event.severity);

    // Build structured_data JSON
    event.structured_data = "{\"component\":\"" + component + "\",\"level\":\"" + level +
                           "\",\"pid\":\"" + pid + "\"}";
    event.log_content = line;

    return true;
}

bool HdfsParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int hdfs_lines = 0;
    int checked = 0;

    while (reader.getLine(line) && checked < 10) {
        // Skip empty lines
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        // Check for HDFS format: 6 digits, space, 6 digits, space, digits, space, LEVEL
        if (line.size() >= 20) {
            bool is_hdfs = true;
            // Check first 6 digits (date)
            for (int i = 0; i < 6 && is_hdfs; i++) {
                if (!std::isdigit(line[i])) is_hdfs = false;
            }
            // Check space
            if (is_hdfs && line[6] != ' ') is_hdfs = false;
            // Check next 6 digits (time)
            for (int i = 7; i < 13 && is_hdfs; i++) {
                if (!std::isdigit(line[i])) is_hdfs = false;
            }
            // Check space
            if (is_hdfs && line[13] != ' ') is_hdfs = false;

            // Check for INFO/WARN/ERROR after PID
            if (is_hdfs) {
                if (line.find(" INFO ", 14) != std::string::npos ||
                    line.find(" WARN ", 14) != std::string::npos ||
                    line.find(" ERROR ", 14) != std::string::npos ||
                    line.find(" DEBUG ", 14) != std::string::npos ||
                    line.find(" FATAL ", 14) != std::string::npos) {
                    hdfs_lines++;
                }
            }
        }
    }

    return hdfs_lines > 0 && hdfs_lines >= (checked / 3);
}

std::vector<ValidationEvent> HdfsParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int64_t event_id = 1;

    while (reader.getLine(line)) {
        int line_number = reader.lineNumber();

        // Remove trailing whitespace
        size_t end = line.find_last_not_of("\r\n");
        if (end != std::string::npos) {
            line = line.substr(0, end + 1);
        }

        if (line.empty()) continue;

        ValidationEvent event;
        if (ParseHdfsLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
