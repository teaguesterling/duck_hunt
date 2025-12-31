#include "zookeeper_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

static std::string MapZkLevel(const std::string& level) {
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

// Parse Zookeeper log line using string operations (no regex to avoid backtracking)
// Format: YYYY-MM-DD HH:MM:SS,mmm - LEVEL  [thread/context] - message
static bool ParseZookeeperLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    if (line.size() < 35) return false;

    size_t pos = 0;

    // Timestamp: YYYY-MM-DD HH:MM:SS,mmm
    // YYYY
    for (int i = 0; i < 4; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }
    if (line[pos] != '-') return false;
    pos++;
    // MM
    for (int i = 0; i < 2; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }
    if (line[pos] != '-') return false;
    pos++;
    // DD
    for (int i = 0; i < 2; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }
    if (line[pos] != ' ') return false;
    pos++;
    // HH
    for (int i = 0; i < 2; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }
    if (line[pos] != ':') return false;
    pos++;
    // MM
    for (int i = 0; i < 2; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }
    if (line[pos] != ':') return false;
    pos++;
    // SS
    for (int i = 0; i < 2; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }
    if (line[pos] != ',') return false;
    pos++;
    // mmm
    for (int i = 0; i < 3; i++) {
        if (pos >= line.size() || !std::isdigit(line[pos])) return false;
        pos++;
    }

    std::string timestamp = line.substr(0, pos);

    // Expect " - " separator
    if (pos + 3 > line.size() || line.substr(pos, 3) != " - ") return false;
    pos += 3;

    // Level: alphabetic word (may have trailing spaces)
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

    // Skip trailing spaces after level
    while (pos < line.size() && line[pos] == ' ') {
        pos++;
    }

    // Thread/context in brackets: [...]
    // Note: can contain nested brackets like [QuorumPeer[myid=1]/...]
    if (pos >= line.size() || line[pos] != '[') return false;
    pos++;

    int bracket_depth = 1;
    size_t thread_start = pos;
    while (pos < line.size() && bracket_depth > 0) {
        if (line[pos] == '[') bracket_depth++;
        else if (line[pos] == ']') bracket_depth--;
        pos++;
    }
    if (bracket_depth != 0) return false;
    std::string thread = line.substr(thread_start, pos - thread_start - 1);

    // Expect " - " separator before message
    if (pos + 3 > line.size() || line.substr(pos, 3) != " - ") return false;
    pos += 3;

    // Message: rest of line
    std::string message = (pos < line.size()) ? line.substr(pos) : "";

    event.event_id = event_id;
    event.tool_name = "zookeeper";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.ref_line = -1;
    event.ref_column = -1;

    event.started_at = timestamp;
    event.category = "zookeeper";
    event.message = message;
    event.severity = MapZkLevel(level);
    event.status = MapLevelToStatus(event.severity);

    // Extract class name from thread if present (e.g., FastLeaderElection@774)
    std::string class_name;
    size_t at_pos = thread.find('@');
    if (at_pos != std::string::npos) {
        size_t class_start = thread.rfind(':', at_pos);
        if (class_start != std::string::npos) {
            class_name = thread.substr(class_start + 1, at_pos - class_start - 1);
        } else {
            class_name = thread.substr(0, at_pos);
        }
    }

    // Build structured_data JSON (escape special chars in thread)
    std::string escaped_thread;
    for (char c : thread) {
        if (c == '"') escaped_thread += "\\\"";
        else if (c == '\\') escaped_thread += "\\\\";
        else escaped_thread += c;
    }

    event.structured_data = "{\"thread\":\"" + escaped_thread + "\",\"level\":\"" + level + "\"";
    if (!class_name.empty()) {
        event.structured_data += ",\"class\":\"" + class_name + "\"";
    }
    event.structured_data += "}";

    event.log_content = line;

    return true;
}

bool ZookeeperParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int zk_lines = 0;
    int checked = 0;

    while (reader.getLine(line) && checked < 10) {
        // Skip empty lines
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        // Check for Zookeeper format: YYYY-MM-DD HH:MM:SS,mmm - LEVEL  [
        if (line.size() >= 30) {
            // Check timestamp pattern
            bool is_zk = true;
            for (int i = 0; i < 4 && is_zk; i++) {
                if (!std::isdigit(line[i])) is_zk = false;
            }
            if (is_zk && line[4] != '-') is_zk = false;
            if (is_zk && (!std::isdigit(line[5]) || !std::isdigit(line[6]))) is_zk = false;
            if (is_zk && line[7] != '-') is_zk = false;
            if (is_zk && (!std::isdigit(line[8]) || !std::isdigit(line[9]))) is_zk = false;

            // Check for " - " separator after timestamp and level in brackets
            if (is_zk) {
                // Look for " - INFO  [" or " - WARN  [" etc.
                if ((line.find(" - INFO", 20) != std::string::npos ||
                     line.find(" - WARN", 20) != std::string::npos ||
                     line.find(" - ERROR", 20) != std::string::npos ||
                     line.find(" - DEBUG", 20) != std::string::npos) &&
                    line.find('[', 25) != std::string::npos) {
                    zk_lines++;
                }
            }
        }
    }

    return zk_lines > 0 && zk_lines >= (checked / 3);
}

std::vector<ValidationEvent> ZookeeperParser::parse(const std::string& content) const {
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
        if (ParseZookeeperLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
