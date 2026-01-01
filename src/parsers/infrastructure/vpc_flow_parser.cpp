#include "vpc_flow_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>
#include <vector>

namespace duckdb {

// AWS VPC Flow Logs format (v2):
// "2 123456789012 eni-abc123 10.0.0.1 192.168.1.100 22 54321 6 10 5000 1609459200 1609459260 ACCEPT OK"
// Fields: version account-id interface-id srcaddr dstaddr srcport dstport protocol packets bytes start end action
// log-status

// Protocol numbers
static std::string ProtocolName(const std::string &proto_num) {
	if (proto_num == "1")
		return "ICMP";
	if (proto_num == "6")
		return "TCP";
	if (proto_num == "17")
		return "UDP";
	if (proto_num == "47")
		return "GRE";
	if (proto_num == "50")
		return "ESP";
	if (proto_num == "51")
		return "AH";
	if (proto_num == "58")
		return "ICMPv6";
	return proto_num;
}

static std::vector<std::string> SplitWhitespace(const std::string &line) {
	std::vector<std::string> tokens;
	std::istringstream iss(line);
	std::string token;
	while (iss >> token) {
		tokens.push_back(token);
	}
	return tokens;
}

static bool ParseVpcFlowLine(const std::string &line, ValidationEvent &event, int64_t event_id, int line_number,
                             const std::vector<std::string> &headers) {
	auto fields = SplitWhitespace(line);

	// VPC Flow Logs v2 has 14 default fields
	// Skip header line
	if (fields.size() < 10)
		return false;
	if (fields[0] == "version")
		return false; // Header line

	// Check if first field is version number (should be "2" or higher)
	bool is_valid = false;
	try {
		int version = std::stoi(fields[0]);
		is_valid = (version >= 2);
	} catch (...) {
		return false;
	}
	if (!is_valid)
		return false;

	event.event_id = event_id;
	event.tool_name = "vpc_flow";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	// Default field positions (v2 format)
	std::string version = fields.size() > 0 ? fields[0] : "";
	std::string account_id = fields.size() > 1 ? fields[1] : "";
	std::string interface_id = fields.size() > 2 ? fields[2] : "";
	std::string srcaddr = fields.size() > 3 ? fields[3] : "";
	std::string dstaddr = fields.size() > 4 ? fields[4] : "";
	std::string srcport = fields.size() > 5 ? fields[5] : "";
	std::string dstport = fields.size() > 6 ? fields[6] : "";
	std::string protocol = fields.size() > 7 ? fields[7] : "";
	std::string packets = fields.size() > 8 ? fields[8] : "";
	std::string bytes = fields.size() > 9 ? fields[9] : "";
	std::string start_time = fields.size() > 10 ? fields[10] : "";
	std::string end_time = fields.size() > 11 ? fields[11] : "";
	std::string action = fields.size() > 12 ? fields[12] : "";
	std::string log_status = fields.size() > 13 ? fields[13] : "";

	// Handle "-" values (indicates no data)
	if (srcaddr == "-")
		srcaddr = "";
	if (dstaddr == "-")
		dstaddr = "";
	if (srcport == "-")
		srcport = "";
	if (dstport == "-")
		dstport = "";

	// Convert Unix timestamp to readable format
	if (!start_time.empty() && start_time != "-") {
		try {
			time_t ts = std::stoll(start_time);
			char buf[64];
			struct tm *tm_info = gmtime(&ts);
			strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
			event.started_at = buf;
		} catch (...) {
			event.started_at = start_time;
		}
	}

	// Determine severity based on action
	if (action == "REJECT") {
		event.severity = "warning";
		event.status = ValidationEventStatus::WARNING;
	} else {
		event.severity = "info";
		event.status = ValidationEventStatus::INFO;
	}

	// Build message
	std::string proto_name = ProtocolName(protocol);
	std::string msg = action + ": ";
	if (!srcaddr.empty()) {
		msg += srcaddr;
		if (!srcport.empty() && srcport != "0")
			msg += ":" + srcport;
	}
	msg += " -> ";
	if (!dstaddr.empty()) {
		msg += dstaddr;
		if (!dstport.empty() && dstport != "0")
			msg += ":" + dstport;
	}
	msg += " (" + proto_name + ")";
	if (!packets.empty() && packets != "-") {
		msg += " " + packets + " pkts";
	}
	if (!bytes.empty() && bytes != "-") {
		msg += " " + bytes + " bytes";
	}
	event.message = msg;

	// Map to schema fields
	event.origin = srcaddr;
	event.principal = account_id;
	event.category = interface_id;

	// Build structured_data JSON
	std::string json = "{";
	json += "\"version\":" + version;
	if (!account_id.empty())
		json += ",\"account_id\":\"" + account_id + "\"";
	if (!interface_id.empty())
		json += ",\"interface_id\":\"" + interface_id + "\"";
	if (!srcaddr.empty())
		json += ",\"srcaddr\":\"" + srcaddr + "\"";
	if (!dstaddr.empty())
		json += ",\"dstaddr\":\"" + dstaddr + "\"";
	if (!srcport.empty())
		json += ",\"srcport\":\"" + srcport + "\"";
	if (!dstport.empty())
		json += ",\"dstport\":\"" + dstport + "\"";
	if (!protocol.empty())
		json += ",\"protocol\":\"" + proto_name + "\"";
	if (!packets.empty() && packets != "-")
		json += ",\"packets\":\"" + packets + "\"";
	if (!bytes.empty() && bytes != "-")
		json += ",\"bytes\":\"" + bytes + "\"";
	if (!action.empty())
		json += ",\"action\":\"" + action + "\"";
	if (!log_status.empty())
		json += ",\"log_status\":\"" + log_status + "\"";
	json += "}";
	event.structured_data = json;

	event.log_content = line;
	return true;
}

bool VpcFlowParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	std::istringstream stream(content);
	std::string line;
	int flow_lines = 0;
	int checked = 0;

	// Pattern to detect VPC Flow Logs
	// Look for lines starting with version number followed by account ID pattern
	static std::regex flow_detect(R"(^2\s+\d{12}\s+eni-)", std::regex::optimize);
	// Also check for header line
	static std::regex header_detect(R"(^version\s+account-id\s+interface-id)", std::regex::optimize);

	while (std::getline(stream, line) && checked < 15) {
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);
		if (line.empty())
			continue;

		checked++;

		if (std::regex_search(line, flow_detect) || std::regex_search(line, header_detect)) {
			flow_lines++;
		}
	}

	return flow_lines > 0 && flow_lines >= (checked / 4);
}

std::vector<ValidationEvent> VpcFlowParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int line_number = 0;
	std::vector<std::string> headers;

	while (std::getline(stream, line)) {
		line_number++;

		size_t end = line.find_last_not_of(" \t\r\n");
		if (end != std::string::npos) {
			line = line.substr(0, end + 1);
		}

		if (line.empty())
			continue;

		// Check for header line
		if (line.find("version") == 0 && line.find("account-id") != std::string::npos) {
			headers = SplitWhitespace(line);
			continue;
		}

		ValidationEvent event;
		if (ParseVpcFlowLine(line, event, event_id, line_number, headers)) {
			events.push_back(event);
			event_id++;
		}
	}

	return events;
}

} // namespace duckdb
