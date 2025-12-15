#include "nginx_access_parser.hpp"
#include "core/legacy_parser_registry.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

static std::string MapStatusCodeToSeverity(int status_code) {
    if (status_code >= 500) {
        return "error";
    } else if (status_code >= 400) {
        return "warning";
    }
    return "info";
}

static ValidationEventStatus MapStatusCodeToStatus(int status_code) {
    if (status_code >= 500) {
        return ValidationEventStatus::ERROR;
    } else if (status_code >= 400) {
        return ValidationEventStatus::WARNING;
    }
    return ValidationEventStatus::INFO;
}

static bool ParseNginxAccessLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // NGINX combined format with optional request time at the end:
    // IP - user [timestamp] "method path protocol" status size "referrer" "user-agent" [request_time]
    static std::regex nginx_pattern(
        R"(^(\S+)\s+\S+\s+\S+\s+\[([^\]]+)\]\s+\"(\S+)\s+(\S+)\s+([^\"]*)\"\s+(\d+)\s+(\d+|-)(?:\s+\"([^\"]*)\"\s+\"([^\"]*)\")?(?:\s+(\d+\.?\d*))?)",
        std::regex::optimize
    );

    std::smatch match;
    if (!std::regex_match(line, match, nginx_pattern)) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "nginx_access";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.line_number = -1;
    event.column_number = -1;

    // Extract fields
    std::string ip_address = match[1].str();
    std::string timestamp = match[2].str();
    std::string method = match[3].str();
    std::string path = match[4].str();
    std::string protocol = match[5].str();
    std::string status_str = match[6].str();
    std::string size_str = match[7].str();
    std::string referrer = (match[8].matched && match[8].str() != "-") ? match[8].str() : "";
    std::string user_agent = match[9].matched ? match[9].str() : "";

    // Field mappings - using new Phase 4 columns
    event.started_at = timestamp;              // Timestamp (proper column)
    event.file_path = path;                    // Request path = "file" being accessed
    event.category = method;                   // HTTP method as category
    event.error_code = status_str;             // Status code as error_code
    event.message = method + " " + path;       // Human-readable summary
    event.origin = ip_address;                 // Client IP address
    // principal: authenticated user not captured in basic regex (usually "-")

    // Status code determines severity
    int status_code = 0;
    try {
        status_code = std::stoi(status_str);
    } catch (...) {}
    event.severity = MapStatusCodeToSeverity(status_code);
    event.status = MapStatusCodeToStatus(status_code);

    // Request time (optional, nginx extended format) - perfect fit for execution_time
    if (match[10].matched && !match[10].str().empty()) {
        try {
            event.execution_time = std::stod(match[10].str());
        } catch (...) {
            event.execution_time = 0.0;
        }
    } else {
        event.execution_time = 0.0;
    }

    // Build structured_data JSON for fields without natural mappings
    std::string json = "{";
    json += "\"ip_address\":\"" + ip_address + "\"";
    json += ",\"protocol\":\"" + protocol + "\"";
    if (size_str != "-") {
        json += ",\"response_bytes\":" + size_str;
    }
    if (!referrer.empty()) {
        std::string escaped_referrer;
        for (char c : referrer) {
            if (c == '"') escaped_referrer += "\\\"";
            else if (c == '\\') escaped_referrer += "\\\\";
            else escaped_referrer += c;
        }
        json += ",\"referrer\":\"" + escaped_referrer + "\"";
    }
    if (!user_agent.empty()) {
        std::string escaped_ua;
        for (char c : user_agent) {
            if (c == '"') escaped_ua += "\\\"";
            else if (c == '\\') escaped_ua += "\\\\";
            else escaped_ua += c;
        }
        json += ",\"user_agent\":\"" + escaped_ua + "\"";
    }
    json += "}";
    event.structured_data = json;

    event.raw_output = line;
    return true;
}

bool NginxAccessParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    int access_lines = 0;
    int checked = 0;

    // Pattern to detect access log format: IP [timestamp] "REQUEST"
    static std::regex access_pattern(R"(^\S+\s+\S+\s+\S+\s+\[[^\]]+\]\s+\")");

    while (std::getline(stream, line) && checked < 5) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, access_pattern)) {
            access_lines++;
        }
    }

    return access_lines > 0 && access_lines >= (checked / 2);
}

std::vector<ValidationEvent> NginxAccessParser::parse(const std::string& content) const {
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
        if (ParseNginxAccessLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

REGISTER_PARSER(NginxAccessParser);

} // namespace duckdb
