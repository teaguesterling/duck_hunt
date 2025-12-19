#include "cisco_asa_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>
#include <unordered_map>

namespace duckdb {

// Cisco ASA severity levels
static std::string MapAsaSeverity(int level) {
    // ASA levels: 0=emergencies, 1=alerts, 2=critical, 3=errors, 4=warnings, 5=notifications, 6=informational, 7=debugging
    if (level <= 3) {
        return "error";
    } else if (level == 4) {
        return "warning";
    }
    return "info";
}

static ValidationEventStatus MapSeverityToStatus(const std::string& severity) {
    if (severity == "error") {
        return ValidationEventStatus::ERROR;
    } else if (severity == "warning") {
        return ValidationEventStatus::WARNING;
    }
    return ValidationEventStatus::INFO;
}

// Parse Cisco ASA syslog format:
// "Jan 15 2025 10:30:45 hostname : %ASA-6-302013: Built inbound TCP connection 12345 for outside:192.168.1.100/54321 to inside:10.0.0.1/22"
// Also handles:
// "%ASA-4-106023: Deny tcp src outside:192.168.1.100/54321 dst inside:10.0.0.1/22"

static bool ParseCiscoAsaLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Look for ASA message ID pattern
    static std::regex asa_pattern(
        R"(%ASA-(\d)-(\d+):\s*(.*))",
        std::regex::optimize
    );

    std::smatch match;
    if (!std::regex_search(line, match, asa_pattern)) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "cisco_asa";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.ref_line = -1;
    event.ref_column = -1;

    int severity_level = std::stoi(match[1].str());
    std::string message_id = match[2].str();
    std::string message_text = match[3].str();

    event.severity = MapAsaSeverity(severity_level);
    event.status = MapSeverityToStatus(event.severity);
    event.error_code = "ASA-" + match[1].str() + "-" + message_id;
    event.message = message_text;

    // Extract timestamp from syslog prefix
    static std::regex ts_pattern(R"(^(\w{3}\s+\d+\s+\d{4}\s+\d{2}:\d{2}:\d{2}))", std::regex::optimize);
    std::smatch ts_match;
    if (std::regex_search(line, ts_match, ts_pattern)) {
        event.started_at = ts_match[1].str();
    } else {
        // Try alternate format without year
        static std::regex ts_pattern2(R"(^(\w{3}\s+\d+\s+\d{2}:\d{2}:\d{2}))", std::regex::optimize);
        if (std::regex_search(line, ts_match, ts_pattern2)) {
            event.started_at = ts_match[1].str();
        }
    }

    // Extract hostname
    static std::regex host_pattern(R"(^\w{3}\s+\d+\s+(?:\d{4}\s+)?\d{2}:\d{2}:\d{2}\s+(\S+))", std::regex::optimize);
    std::smatch host_match;
    if (std::regex_search(line, host_match, host_pattern)) {
        event.category = host_match[1].str();
    }

    // Try to extract IP addresses from the message
    // Common patterns: "src outside:192.168.1.100/54321" or "for outside:192.168.1.100/54321"
    static std::regex src_pattern(R"((?:src|from)\s+\w+:(\d+\.\d+\.\d+\.\d+)/(\d+))", std::regex::optimize | std::regex::icase);
    static std::regex dst_pattern(R"((?:dst|to)\s+\w+:(\d+\.\d+\.\d+\.\d+)/(\d+))", std::regex::optimize | std::regex::icase);

    std::string src_ip, src_port, dst_ip, dst_port;
    std::smatch ip_match;
    if (std::regex_search(message_text, ip_match, src_pattern)) {
        src_ip = ip_match[1].str();
        src_port = ip_match[2].str();
        event.origin = src_ip;
    }
    if (std::regex_search(message_text, ip_match, dst_pattern)) {
        dst_ip = ip_match[1].str();
        dst_port = ip_match[2].str();
    }

    // Determine action from common message IDs
    std::string action = "unknown";
    // 106xxx = ACL deny/permit
    // 302xxx = connection built/teardown
    // 305xxx = NAT
    // 313xxx = ICMP
    // 710xxx = TCP/UDP connection
    if (message_text.find("Deny") != std::string::npos || message_text.find("deny") != std::string::npos) {
        action = "deny";
    } else if (message_text.find("Built") != std::string::npos) {
        action = "allow";
    } else if (message_text.find("Teardown") != std::string::npos) {
        action = "teardown";
    } else if (message_text.find("Permitted") != std::string::npos || message_text.find("permitted") != std::string::npos) {
        action = "permit";
    }

    // Build structured_data JSON
    std::string json = "{";
    json += "\"severity_level\":" + std::to_string(severity_level);
    json += ",\"message_id\":\"" + message_id + "\"";
    json += ",\"action\":\"" + action + "\"";
    if (!src_ip.empty()) {
        json += ",\"src\":\"" + src_ip + "\"";
        json += ",\"src_port\":\"" + src_port + "\"";
    }
    if (!dst_ip.empty()) {
        json += ",\"dst\":\"" + dst_ip + "\"";
        json += ",\"dst_port\":\"" + dst_port + "\"";
    }
    json += "}";
    event.structured_data = json;

    event.log_content = line;
    return true;
}

bool CiscoAsaParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    int asa_lines = 0;
    int checked = 0;

    // Pattern to detect Cisco ASA message IDs
    static std::regex asa_detect(R"(%ASA-\d-\d+:)", std::regex::optimize);

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, asa_detect)) {
            asa_lines++;
        }
    }

    return asa_lines > 0 && asa_lines >= (checked / 3);
}

std::vector<ValidationEvent> CiscoAsaParser::parse(const std::string& content) const {
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
        if (ParseCiscoAsaLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
