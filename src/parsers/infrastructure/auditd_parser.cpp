#include "auditd_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>
#include <unordered_map>

namespace duckdb {

// Linux Audit daemon (auditd) log formats:
// 1. auditd format:
//    "type=SYSCALL msg=audit(1610721045.123:456): arch=c000003e syscall=59 success=yes exit=0 ... comm=\"sudo\" exe=\"/usr/bin/sudo\" key=\"privileged\""
//
// 2. SSH auth log format (often parsed alongside auditd):
//    "Jan 15 10:30:45 hostname sshd[1234]: Accepted publickey for user from 192.168.1.100 port 54321 ssh2"
//    "Jan 15 10:30:45 hostname sshd[1234]: Failed password for invalid user admin from 192.168.1.100 port 54321 ssh2"

static std::unordered_map<std::string, std::string> ParseAuditFields(const std::string& line) {
    std::unordered_map<std::string, std::string> fields;

    // Pattern for key=value or key="value with spaces"
    static std::regex kv_pattern(
        R"(([a-zA-Z_][a-zA-Z0-9_]*)=(?:\"([^\"]*)\"|([^\s]*)))",
        std::regex::optimize
    );

    auto begin = std::sregex_iterator(line.begin(), line.end(), kv_pattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::string key = (*it)[1].str();
        std::string value = (*it)[2].matched ? (*it)[2].str() : (*it)[3].str();
        fields[key] = value;
    }

    return fields;
}

static bool ParseAuditdLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Auditd format: type=TYPE msg=audit(timestamp:serial): fields...
    static std::regex audit_pattern(
        R"(type=(\S+)\s+msg=audit\((\d+\.\d+):(\d+)\):\s*(.*))",
        std::regex::optimize
    );

    std::smatch match;
    if (std::regex_search(line, match, audit_pattern)) {
        event.event_id = event_id;
        event.tool_name = "auditd";
        event.event_type = ValidationEventType::DEBUG_INFO;
        event.log_line_start = line_number;
        event.log_line_end = line_number;
        event.execution_time = 0.0;
        event.line_number = -1;
        event.column_number = -1;

        std::string audit_type = match[1].str();
        std::string timestamp = match[2].str();
        std::string serial = match[3].str();
        std::string fields_str = match[4].str();

        auto fields = ParseAuditFields(fields_str);

        // Convert Unix timestamp
        try {
            time_t ts = std::stoll(timestamp.substr(0, timestamp.find('.')));
            char buf[64];
            struct tm* tm_info = gmtime(&ts);
            strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
            event.started_at = buf;
        } catch (...) {
            event.started_at = timestamp;
        }

        // Determine severity based on type and success
        std::string success = fields.count("success") ? fields["success"] : "";
        if (audit_type.find("AVC") != std::string::npos ||
            audit_type.find("SELINUX") != std::string::npos ||
            success == "no") {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        } else {
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
        }

        event.category = audit_type;
        event.error_code = audit_type;

        // Build message from key fields
        std::string msg = audit_type;
        if (fields.count("comm")) msg += " comm=" + fields["comm"];
        if (fields.count("exe")) msg += " exe=" + fields["exe"];
        if (fields.count("key")) msg += " key=" + fields["key"];
        if (!success.empty()) msg += " success=" + success;
        event.message = msg;

        // Extract principal (user info)
        if (fields.count("auid")) {
            event.principal = "auid=" + fields["auid"];
            if (fields.count("uid")) event.principal += " uid=" + fields["uid"];
        } else if (fields.count("uid")) {
            event.principal = "uid=" + fields["uid"];
        }

        // Extract file path if present
        if (fields.count("exe")) {
            event.file_path = fields["exe"];
        } else if (fields.count("name")) {
            event.file_path = fields["name"];
        }

        // Build structured_data JSON
        std::string json = "{";
        json += "\"type\":\"" + audit_type + "\"";
        json += ",\"serial\":\"" + serial + "\"";
        for (const auto& pair : fields) {
            // Escape value
            std::string escaped;
            for (char c : pair.second) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else escaped += c;
            }
            json += ",\"" + pair.first + "\":\"" + escaped + "\"";
        }
        json += "}";
        event.structured_data = json;

        event.raw_output = line;
        return true;
    }

    // Try SSH auth log format
    static std::regex ssh_pattern(
        R"(^(\w{3}\s+\d+\s+\d{2}:\d{2}:\d{2})\s+(\S+)\s+sshd\[(\d+)\]:\s*(.*)$)",
        std::regex::optimize
    );

    if (std::regex_match(line, match, ssh_pattern)) {
        event.event_id = event_id;
        event.tool_name = "auditd";
        event.event_type = ValidationEventType::DEBUG_INFO;
        event.log_line_start = line_number;
        event.log_line_end = line_number;
        event.execution_time = 0.0;
        event.line_number = -1;
        event.column_number = -1;

        std::string timestamp = match[1].str();
        std::string hostname = match[2].str();
        std::string pid = match[3].str();
        std::string message = match[4].str();

        event.started_at = timestamp;
        event.category = "sshd";
        event.message = message;

        // Determine severity based on message content
        if (message.find("Failed") != std::string::npos ||
            message.find("Invalid") != std::string::npos ||
            message.find("error") != std::string::npos ||
            message.find("refused") != std::string::npos) {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        } else if (message.find("Accepted") != std::string::npos ||
                   message.find("session opened") != std::string::npos) {
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
        } else {
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
        }

        // Extract IP address if present
        static std::regex ip_pattern(R"(from\s+(\d+\.\d+\.\d+\.\d+))");
        std::smatch ip_match;
        if (std::regex_search(message, ip_match, ip_pattern)) {
            event.origin = ip_match[1].str();
        }

        // Extract username if present
        static std::regex user_pattern(R"(for\s+(?:invalid\s+user\s+)?(\S+)\s+from)");
        std::smatch user_match;
        if (std::regex_search(message, user_match, user_pattern)) {
            event.principal = user_match[1].str();
        }

        // Build structured_data JSON
        std::string json = "{";
        json += "\"hostname\":\"" + hostname + "\"";
        json += ",\"pid\":\"" + pid + "\"";
        json += ",\"service\":\"sshd\"";
        if (!event.origin.empty()) json += ",\"source_ip\":\"" + event.origin + "\"";
        if (!event.principal.empty()) json += ",\"user\":\"" + event.principal + "\"";
        json += "}";
        event.structured_data = json;

        event.raw_output = line;
        return true;
    }

    return false;
}

bool AuditdParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    int audit_lines = 0;
    int checked = 0;

    // Patterns to detect audit logs
    static std::regex audit_detect(R"(type=\S+\s+msg=audit\()", std::regex::optimize);
    static std::regex ssh_detect(R"(sshd\[\d+\]:)", std::regex::optimize);

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, audit_detect) || std::regex_search(line, ssh_detect)) {
            audit_lines++;
        }
    }

    return audit_lines > 0 && audit_lines >= (checked / 3);
}

std::vector<ValidationEvent> AuditdParser::parse(const std::string& content) const {
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
        if (ParseAuditdLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
