#include "iptables_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>
#include <unordered_map>

namespace duckdb {

// Parse iptables/netfilter kernel log format:
// "Jan 15 10:30:45 hostname kernel: [12345.678901] IN=eth0 OUT= MAC=... SRC=192.168.1.100 DST=10.0.0.1 ... PROTO=TCP SPT=54321 DPT=22 ..."
// Also handles UFW format:
// "Jan 15 10:30:45 hostname kernel: [UFW BLOCK] IN=eth0 OUT= MAC=... SRC=..."

static std::unordered_map<std::string, std::string> ParseIptablesFields(const std::string& line) {
    std::unordered_map<std::string, std::string> fields;

    // Pattern for KEY=VALUE pairs (value may be empty)
    static std::regex kv_pattern(R"(([A-Z]+)=([^\s]*))", std::regex::optimize);

    auto begin = std::sregex_iterator(line.begin(), line.end(), kv_pattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        fields[(*it)[1].str()] = (*it)[2].str();
    }

    return fields;
}

static bool ParseIptablesLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Must contain kernel log markers and network fields
    if (line.find("kernel:") == std::string::npos &&
        line.find("kernel[") == std::string::npos) {
        return false;
    }

    // Must have at least SRC or IN field to be a firewall log
    if (line.find("SRC=") == std::string::npos && line.find("IN=") == std::string::npos) {
        return false;
    }

    auto fields = ParseIptablesFields(line);
    if (fields.empty()) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "iptables";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    // Extract timestamp from syslog prefix
    static std::regex timestamp_pattern(R"(^(\w{3}\s+\d+\s+\d{2}:\d{2}:\d{2}))", std::regex::optimize);
    std::smatch ts_match;
    if (std::regex_search(line, ts_match, timestamp_pattern)) {
        event.started_at = ts_match[1].str();
    }

    // Extract hostname
    static std::regex hostname_pattern(R"(^\w{3}\s+\d+\s+\d{2}:\d{2}:\d{2}\s+(\S+))", std::regex::optimize);
    std::smatch host_match;
    if (std::regex_search(line, host_match, hostname_pattern)) {
        event.category = host_match[1].str();
    }

    // Determine action (UFW BLOCK/ALLOW, or infer from context)
    std::string action = "log";
    if (line.find("[UFW BLOCK]") != std::string::npos) {
        action = "block";
        event.severity = "warning";
        event.status = ValidationEventStatus::WARNING;
    } else if (line.find("[UFW ALLOW]") != std::string::npos) {
        action = "allow";
        event.severity = "info";
        event.status = ValidationEventStatus::INFO;
    } else if (line.find("[UFW AUDIT]") != std::string::npos) {
        action = "audit";
        event.severity = "info";
        event.status = ValidationEventStatus::INFO;
    } else if (line.find("BLOCK") != std::string::npos || line.find("DROP") != std::string::npos) {
        action = "block";
        event.severity = "warning";
        event.status = ValidationEventStatus::WARNING;
    } else if (line.find("REJECT") != std::string::npos) {
        action = "reject";
        event.severity = "warning";
        event.status = ValidationEventStatus::WARNING;
    } else {
        event.severity = "info";
        event.status = ValidationEventStatus::INFO;
    }

    // Build message
    std::string src = fields.count("SRC") ? fields["SRC"] : "";
    std::string dst = fields.count("DST") ? fields["DST"] : "";
    std::string proto = fields.count("PROTO") ? fields["PROTO"] : "";
    std::string spt = fields.count("SPT") ? fields["SPT"] : "";
    std::string dpt = fields.count("DPT") ? fields["DPT"] : "";
    std::string in_iface = fields.count("IN") ? fields["IN"] : "";
    std::string out_iface = fields.count("OUT") ? fields["OUT"] : "";

    std::string msg = action + ": ";
    if (!src.empty()) {
        msg += src;
        if (!spt.empty()) msg += ":" + spt;
    }
    msg += " -> ";
    if (!dst.empty()) {
        msg += dst;
        if (!dpt.empty()) msg += ":" + dpt;
    }
    if (!proto.empty()) {
        msg += " (" + proto + ")";
    }
    event.message = msg;

    // Map to schema fields
    // origin = source IP (network origin)
    event.origin = src;

    // Build structured_data JSON
    std::string json = "{";
    json += "\"action\":\"" + action + "\"";
    if (!src.empty()) json += ",\"src\":\"" + src + "\"";
    if (!dst.empty()) json += ",\"dst\":\"" + dst + "\"";
    if (!proto.empty()) json += ",\"proto\":\"" + proto + "\"";
    if (!spt.empty()) json += ",\"src_port\":\"" + spt + "\"";
    if (!dpt.empty()) json += ",\"dst_port\":\"" + dpt + "\"";
    if (!in_iface.empty()) json += ",\"in_interface\":\"" + in_iface + "\"";
    if (!out_iface.empty()) json += ",\"out_interface\":\"" + out_iface + "\"";
    if (fields.count("MAC")) json += ",\"mac\":\"" + fields["MAC"] + "\"";
    if (fields.count("TTL")) json += ",\"ttl\":\"" + fields["TTL"] + "\"";
    if (fields.count("LEN")) json += ",\"length\":\"" + fields["LEN"] + "\"";
    json += "}";
    event.structured_data = json;

    event.raw_output = line;
    return true;
}

bool IptablesParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    int iptables_lines = 0;
    int checked = 0;

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        // Look for kernel firewall log markers
        bool has_kernel = (line.find("kernel:") != std::string::npos ||
                          line.find("kernel[") != std::string::npos);
        bool has_fields = (line.find("SRC=") != std::string::npos ||
                          line.find("IN=") != std::string::npos);

        if (has_kernel && has_fields) {
            iptables_lines++;
        }
    }

    return iptables_lines > 0 && iptables_lines >= (checked / 3);
}

std::vector<ValidationEvent> IptablesParser::parse(const std::string& content) const {
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
        if (ParseIptablesLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
