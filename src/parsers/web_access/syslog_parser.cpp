#include "syslog_parser.hpp"
#include "../../core/parser_registry.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>
#include <unordered_map>

namespace duckdb {

// Syslog severity levels (RFC 5424)
static std::string MapSyslogSeverity(int severity_code) {
    switch (severity_code) {
        case 0: return "error";    // Emergency
        case 1: return "error";    // Alert
        case 2: return "error";    // Critical
        case 3: return "error";    // Error
        case 4: return "warning";  // Warning
        case 5: return "info";     // Notice
        case 6: return "info";     // Informational
        case 7: return "info";     // Debug
        default: return "info";
    }
}

static ValidationEventStatus MapSeverityToStatus(const std::string& severity) {
    if (severity == "error") {
        return ValidationEventStatus::ERROR;
    } else if (severity == "warning") {
        return ValidationEventStatus::WARNING;
    }
    return ValidationEventStatus::INFO;
}

// Parse BSD syslog format: "Dec 12 10:15:42 hostname process[pid]: message"
static bool ParseBSDSyslog(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // BSD format: Mon DD HH:MM:SS hostname process[pid]: message
    static std::regex bsd_pattern(
        R"(^([A-Z][a-z]{2})\s+(\d{1,2})\s+(\d{2}:\d{2}:\d{2})\s+(\S+)\s+([^:\[]+)(?:\[(\d+)\])?:\s*(.*)$)",
        std::regex::optimize
    );

    std::smatch match;
    if (!std::regex_match(line, match, bsd_pattern)) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "syslog";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    // Extract fields
    std::string timestamp = match[1].str() + " " + match[2].str() + " " + match[3].str();
    std::string hostname = match[4].str();
    std::string process = match[5].str();
    std::string pid = (match[6].matched && !match[6].str().empty()) ? match[6].str() : "";
    std::string msg = match[7].str();

    // Field mappings (using available schema columns)
    event.function_name = timestamp;           // Timestamp (using function_name since started_at not exposed)
    event.category = process;                  // Process/service name
    event.message = msg;                       // The log message

    // Default severity for BSD (no priority field)
    event.severity = "info";
    event.status = ValidationEventStatus::INFO;

    // Build structured_data JSON
    std::string json = "{";
    json += "\"hostname\":\"" + hostname + "\"";
    if (!pid.empty()) {
        json += ",\"pid\":" + pid;
    }
    json += "}";
    event.structured_data = json;

    event.raw_output = line;
    return true;
}

// Parse RFC 5424 syslog format: "<priority>version timestamp hostname app proc-id msg-id structured-data message"
static bool ParseRFC5424Syslog(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // RFC 5424: <pri>version timestamp hostname app-name procid msgid sd msg
    static std::regex rfc5424_pattern(
        R"(^<(\d+)>(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S*)\s*(.*)$)",
        std::regex::optimize
    );

    std::smatch match;
    if (!std::regex_match(line, match, rfc5424_pattern)) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "syslog";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    // Extract fields
    std::string priority_str = match[1].str();
    std::string timestamp = match[3].str();
    std::string hostname = match[4].str();
    std::string app_name = match[5].str();
    std::string procid = match[6].str();
    std::string msgid = match[7].str();
    std::string sd = match[8].str();
    std::string msg = match[9].str();

    // Parse priority to get severity
    int priority = 0;
    try {
        priority = std::stoi(priority_str);
    } catch (...) {}
    int severity_code = priority & 0x07;  // Lower 3 bits
    int facility_code = (priority >> 3) & 0x1F;  // Upper 5 bits
    event.severity = MapSyslogSeverity(severity_code);
    event.status = MapSeverityToStatus(event.severity);

    // Field mappings (using available schema columns)
    event.function_name = timestamp;           // Timestamp (using function_name since started_at not exposed)
    event.category = app_name;                 // Application name
    event.message = msg;                       // The log message

    // Build structured_data JSON
    std::string json = "{";
    json += "\"hostname\":\"" + hostname + "\"";
    json += ",\"facility\":" + std::to_string(facility_code);
    json += ",\"severity_code\":" + std::to_string(severity_code);
    if (procid != "-") {
        json += ",\"pid\":\"" + procid + "\"";
    }
    if (msgid != "-") {
        json += ",\"msgid\":\"" + msgid + "\"";
    }
    if (!sd.empty() && sd != "-") {
        // Store RFC 5424 structured data as-is
        json += ",\"rfc5424_sd\":\"" + sd + "\"";
    }
    json += "}";
    event.structured_data = json;

    event.raw_output = line;
    return true;
}

// Simple fallback parser for less structured syslog
static bool ParseSimpleSyslog(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Very simple: just look for timestamp-like patterns at the start
    static std::regex simple_pattern(
        R"(^(\S+\s+\d+\s+\d+:\d+:\d+|\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[^\s]*)\s+(.*)$)",
        std::regex::optimize
    );

    std::smatch match;
    if (!std::regex_match(line, match, simple_pattern)) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "syslog";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    event.function_name = match[1].str();      // Timestamp (using function_name since started_at not exposed)
    event.message = match[2].str();
    event.category = "syslog";
    event.severity = "info";
    event.status = ValidationEventStatus::INFO;

    event.raw_output = line;
    return true;
}

bool SyslogParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    int syslog_lines = 0;
    int checked = 0;

    // BSD month pattern
    static std::regex bsd_start(R"(^[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}\s+)");
    // RFC 5424 pattern
    static std::regex rfc5424_start(R"(^<\d+>\d+\s+)");

    while (std::getline(stream, line) && checked < 5) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, bsd_start) || std::regex_search(line, rfc5424_start)) {
            syslog_lines++;
        }
    }

    return syslog_lines > 0 && syslog_lines >= (checked / 2);
}

std::vector<ValidationEvent> SyslogParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int line_number = 0;

    while (std::getline(stream, line)) {
        line_number++;

        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
            line = line.substr(0, end + 1);
        }

        if (line.empty()) continue;

        ValidationEvent event;

        // Try RFC 5424 first (more specific)
        if (ParseRFC5424Syslog(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
        // Then BSD format
        else if (ParseBSDSyslog(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
        // Fallback to simple parser
        else if (ParseSimpleSyslog(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

REGISTER_PARSER(SyslogParser);

} // namespace duckdb
