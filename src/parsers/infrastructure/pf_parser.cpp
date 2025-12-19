#include "pf_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Parse BSD Packet Filter (pf) log format:
// "Jan 15 10:30:45.123456 rule 42/0(match): block in on em0: 192.168.1.100.54321 > 10.0.0.1.22: S"
// Also handles pfSense/OPNsense format:
// "Jan 15 10:30:45 filterlog: 5,16777216,,1000000103,igb0,match,block,in,4,0x0,,64,0,0,DF,17,udp,..."

static bool ParsePfLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Standard pf log format
    static std::regex pf_pattern(
        R"re(^(\w{3}\s+\d+\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?)\s+(?:\S+\s+)?rule\s+(\d+)/(\d+)\((\w+)\):\s+(pass|block|match)\s+(in|out)\s+on\s+(\S+):\s+(\d+\.\d+\.\d+\.\d+)\.(\d+)\s+>\s+(\d+\.\d+\.\d+\.\d+)\.(\d+):\s*(.*))re",
        std::regex::optimize | std::regex::icase
    );

    // pfSense/OPNsense filterlog format (CSV-like)
    static std::regex filterlog_pattern(
        R"(filterlog:\s*(\d+),)",
        std::regex::optimize
    );

    std::smatch match;

    if (std::regex_search(line, match, pf_pattern)) {
        event.event_id = event_id;
        event.tool_name = "pf";
        event.event_type = ValidationEventType::DEBUG_INFO;
        event.log_line_start = line_number;
        event.log_line_end = line_number;
        event.execution_time = 0.0;
        event.ref_line = -1;
        event.ref_column = -1;

        event.started_at = match[1].str();

        std::string rule_num = match[2].str();
        std::string sub_rule = match[3].str();
        std::string match_type = match[4].str();
        std::string action = match[5].str();
        std::string direction = match[6].str();
        std::string interface = match[7].str();
        std::string src_ip = match[8].str();
        std::string src_port = match[9].str();
        std::string dst_ip = match[10].str();
        std::string dst_port = match[11].str();
        std::string flags = match[12].str();

        // Normalize action to lowercase
        for (char& c : action) c = std::tolower(c);

        if (action == "block") {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        } else {
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
        }

        event.message = action + " " + direction + ": " + src_ip + ":" + src_port +
                       " -> " + dst_ip + ":" + dst_port;
        if (!flags.empty()) {
            event.message += " [" + flags + "]";
        }

        event.category = interface;
        event.origin = src_ip;

        // Build structured_data JSON
        std::string json = "{";
        json += "\"action\":\"" + action + "\"";
        json += ",\"direction\":\"" + direction + "\"";
        json += ",\"interface\":\"" + interface + "\"";
        json += ",\"src\":\"" + src_ip + "\"";
        json += ",\"src_port\":\"" + src_port + "\"";
        json += ",\"dst\":\"" + dst_ip + "\"";
        json += ",\"dst_port\":\"" + dst_port + "\"";
        json += ",\"rule\":\"" + rule_num + "/" + sub_rule + "\"";
        json += ",\"match_type\":\"" + match_type + "\"";
        if (!flags.empty()) json += ",\"flags\":\"" + flags + "\"";
        json += "}";
        event.structured_data = json;

        event.log_content = line;
        return true;
    }

    // Try filterlog format (pfSense/OPNsense)
    if (line.find("filterlog:") != std::string::npos) {
        // Parse CSV fields after filterlog:
        size_t start = line.find("filterlog:");
        if (start == std::string::npos) return false;
        start += 10; // length of "filterlog:"

        // Skip whitespace
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) start++;

        std::string csv_part = line.substr(start);
        std::vector<std::string> fields;
        std::stringstream ss(csv_part);
        std::string field;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        // pfSense filterlog has many fields, key ones:
        // 0: rule number, 1: sub-rule, 2: anchor, 3: tracker, 4: interface
        // 5: reason, 6: action, 7: direction, 8: IP version
        // For IPv4: 18: protocol name, 19+: protocol-specific
        if (fields.size() < 10) return false;

        event.event_id = event_id;
        event.tool_name = "pf";
        event.event_type = ValidationEventType::DEBUG_INFO;
        event.log_line_start = line_number;
        event.log_line_end = line_number;
        event.execution_time = 0.0;
        event.ref_line = -1;
        event.ref_column = -1;

        // Extract timestamp from syslog prefix
        static std::regex ts_pattern(R"(^(\w{3}\s+\d+\s+\d{2}:\d{2}:\d{2}))", std::regex::optimize);
        std::smatch ts_match;
        if (std::regex_search(line, ts_match, ts_pattern)) {
            event.started_at = ts_match[1].str();
        }

        std::string interface = fields.size() > 4 ? fields[4] : "";
        std::string action = fields.size() > 6 ? fields[6] : "";
        std::string direction = fields.size() > 7 ? fields[7] : "";

        // Normalize action
        for (char& c : action) c = std::tolower(c);

        if (action == "block") {
            event.severity = "warning";
            event.status = ValidationEventStatus::WARNING;
        } else {
            event.severity = "info";
            event.status = ValidationEventStatus::INFO;
        }

        event.message = action + " " + direction + " on " + interface;
        event.category = interface;

        // Build structured_data JSON
        std::string json = "{";
        json += "\"action\":\"" + action + "\"";
        json += ",\"direction\":\"" + direction + "\"";
        json += ",\"interface\":\"" + interface + "\"";
        if (fields.size() > 0) json += ",\"rule\":\"" + fields[0] + "\"";
        json += "}";
        event.structured_data = json;

        event.log_content = line;
        return true;
    }

    return false;
}

bool PfParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    int pf_lines = 0;
    int checked = 0;

    // Patterns to detect pf-style logging
    static std::regex pf_detect(R"(rule\s+\d+/\d+\(\w+\):\s+(pass|block|match))", std::regex::optimize | std::regex::icase);
    static std::regex filterlog_detect(R"(filterlog:)", std::regex::optimize);

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, pf_detect) || std::regex_search(line, filterlog_detect)) {
            pf_lines++;
        }
    }

    return pf_lines > 0 && pf_lines >= (checked / 3);
}

std::vector<ValidationEvent> PfParser::parse(const std::string& content) const {
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
        if (ParsePfLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
