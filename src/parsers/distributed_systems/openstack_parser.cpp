#include "openstack_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <sstream>

namespace duckdb {

static std::string MapOpenStackLevel(const std::string& level) {
    if (level == "ERROR" || level == "CRITICAL" || level == "FATAL") {
        return "error";
    } else if (level == "WARNING" || level == "WARN") {
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

// Parse OpenStack log line using string operations
// Format variations:
// 1. With log file prefix: logfile.log YYYY-MM-DD HH:MM:SS.mmm PID LEVEL component [req-...] message
// 2. Without prefix: YYYY-MM-DD HH:MM:SS.mmm PID LEVEL component [req-...] message
static bool ParseOpenStackLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    if (line.size() < 40) return false;

    size_t pos = 0;
    std::string log_file;

    // Check if line starts with log file prefix (contains .log)
    // Skip to timestamp by finding YYYY-MM-DD pattern
    size_t timestamp_pos = std::string::npos;

    // Look for YYYY-MM-DD HH:MM:SS pattern
    for (size_t i = 0; i + 23 < line.size(); i++) {
        if (std::isdigit(line[i]) && std::isdigit(line[i+1]) &&
            std::isdigit(line[i+2]) && std::isdigit(line[i+3]) &&
            line[i+4] == '-' &&
            std::isdigit(line[i+5]) && std::isdigit(line[i+6]) &&
            line[i+7] == '-' &&
            std::isdigit(line[i+8]) && std::isdigit(line[i+9]) &&
            line[i+10] == ' ' &&
            std::isdigit(line[i+11]) && std::isdigit(line[i+12]) &&
            line[i+13] == ':') {
            if (i > 0) {
                log_file = line.substr(0, i);
                // Trim trailing space from log_file
                while (!log_file.empty() && log_file.back() == ' ') {
                    log_file.pop_back();
                }
            }
            timestamp_pos = i;
            break;
        }
    }

    if (timestamp_pos == std::string::npos) return false;
    pos = timestamp_pos;

    // Parse timestamp: YYYY-MM-DD HH:MM:SS.mmm
    size_t ts_start = pos;
    // YYYY-MM-DD
    pos += 10;
    if (line[pos] != ' ') return false;
    pos++;
    // HH:MM:SS
    pos += 8;
    // .mmm (optional microseconds)
    if (pos < line.size() && line[pos] == '.') {
        pos++;
        while (pos < line.size() && std::isdigit(line[pos])) {
            pos++;
        }
    }
    std::string timestamp = line.substr(ts_start, pos - ts_start);

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
    if (level != "INFO" && level != "WARNING" && level != "WARN" &&
        level != "ERROR" && level != "CRITICAL" && level != "DEBUG" &&
        level != "TRACE" && level != "AUDIT" && level != "FATAL") {
        return false;
    }

    // Skip space
    if (pos >= line.size() || line[pos] != ' ') return false;
    pos++;

    // Component: identifier with dots (e.g., nova.osapi_compute.wsgi.server)
    size_t comp_start = pos;
    while (pos < line.size() && (std::isalnum(line[pos]) || line[pos] == '.' || line[pos] == '_')) {
        pos++;
    }
    if (pos == comp_start) return false;
    std::string component = line.substr(comp_start, pos - comp_start);

    // Skip space
    if (pos >= line.size() || line[pos] != ' ') return false;
    pos++;

    // Request context in brackets: [req-...]
    std::string request_id;
    if (pos < line.size() && line[pos] == '[') {
        pos++;
        size_t ctx_start = pos;
        int bracket_depth = 1;
        while (pos < line.size() && bracket_depth > 0) {
            if (line[pos] == '[') bracket_depth++;
            else if (line[pos] == ']') bracket_depth--;
            pos++;
        }
        request_id = line.substr(ctx_start, pos - ctx_start - 1);

        // Skip space after context
        if (pos < line.size() && line[pos] == ' ') {
            pos++;
        }
    }

    // Message: rest of line
    std::string message = (pos < line.size()) ? line.substr(pos) : "";

    event.event_id = event_id;
    event.tool_name = "openstack";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.ref_line = -1;
    event.ref_column = -1;

    event.started_at = timestamp;
    event.category = component;
    event.message = message;
    event.severity = MapOpenStackLevel(level);
    event.status = MapLevelToStatus(event.severity);

    // Build structured_data JSON
    event.structured_data = "{\"component\":\"" + component + "\",\"level\":\"" + level +
                           "\",\"pid\":\"" + pid + "\"";
    if (!log_file.empty()) {
        event.structured_data += ",\"log_file\":\"" + log_file + "\"";
    }
    if (!request_id.empty()) {
        // Escape special chars
        std::string escaped_req;
        for (char c : request_id) {
            if (c == '"') escaped_req += "\\\"";
            else if (c == '\\') escaped_req += "\\\\";
            else escaped_req += c;
        }
        event.structured_data += ",\"request_id\":\"" + escaped_req + "\"";
    }
    event.structured_data += "}";

    event.log_content = line;

    return true;
}

bool OpenStackParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    SafeParsing::SafeLineReader reader(content);
    std::string line;
    int os_lines = 0;
    int checked = 0;

    while (reader.getLine(line) && checked < 10) {
        // Skip empty lines
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        // OpenStack indicators:
        // - Contains nova., neutron., cinder., glance., keystone., etc.
        // - Contains [req-...] request context
        // - Has YYYY-MM-DD HH:MM:SS timestamp with PID and level

        bool has_openstack_component = (line.find("nova.") != std::string::npos ||
                                        line.find("neutron.") != std::string::npos ||
                                        line.find("cinder.") != std::string::npos ||
                                        line.find("glance.") != std::string::npos ||
                                        line.find("keystone.") != std::string::npos ||
                                        line.find("swift.") != std::string::npos ||
                                        line.find("heat.") != std::string::npos);

        bool has_request_context = (line.find("[req-") != std::string::npos);

        if (has_openstack_component && has_request_context) {
            os_lines++;
        }
    }

    return os_lines > 0 && os_lines >= (checked / 3);
}

std::vector<ValidationEvent> OpenStackParser::parse(const std::string& content) const {
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
        if (ParseOpenStackLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
