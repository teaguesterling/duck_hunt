#include "s3_access_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// AWS S3 Access Log format:
// bucket_owner bucket [timestamp] remote_ip requester request_id operation key "request_uri" http_status error_code
// bytes_sent object_size total_time turn_around_time "referrer" "user_agent" version_id host_id signature_version
// cipher_suite authentication_type host_header tls_version access_point_arn acl_required
//
// Example:
// 79a59df900b949e55d96a1e698fbacedfd6e09d98eacf8f8d5218e7cd47ef2be mybucket [15/Jan/2025:10:30:45 +0000] 192.168.1.100
// arn:aws:iam::123456789012:user/username ABCDEF123456 REST.GET.OBJECT mykey.txt "GET /mybucket/mykey.txt HTTP/1.1" 200
// - 12345 12345 50 49 "-" "aws-cli/2.0.0" - abc123= SigV4 ECDHE-RSA-AES128-SHA AuthHeader mybucket.s3.amazonaws.com
// TLSv1.2

static bool ParseS3AccessLine(const std::string &line, ValidationEvent &event, int64_t event_id, int line_number) {
	// S3 access logs have a complex format with quoted strings
	// We'll parse the key fields using a combination of regex and position-based parsing

	// First check for the timestamp format [DD/Mon/YYYY:HH:MM:SS +ZZZZ]
	static std::regex timestamp_pattern(R"(\[(\d{2}/\w{3}/\d{4}:\d{2}:\d{2}:\d{2}\s+[+-]\d{4})\])");
	std::smatch ts_match;
	if (!std::regex_search(line, ts_match, timestamp_pattern)) {
		return false;
	}

	event.event_id = event_id;
	event.tool_name = "s3_access";
	event.event_type = ValidationEventType::DEBUG_INFO;
	event.log_line_start = line_number;
	event.log_line_end = line_number;
	event.execution_time = 0.0;
	event.ref_line = -1;
	event.ref_column = -1;

	event.started_at = ts_match[1].str();

	// Parse fields before timestamp
	std::string before_ts = line.substr(0, ts_match.position());
	std::istringstream before_stream(before_ts);
	std::string bucket_owner, bucket;
	before_stream >> bucket_owner >> bucket;

	// Parse fields after timestamp
	std::string after_ts = line.substr(ts_match.position() + ts_match.length());

	// Extract remote IP, requester, request_id, operation, key
	static std::regex after_ts_pattern(
	    R"(^\s*(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+\"([^\"]*)\"\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+))",
	    std::regex::optimize);
	std::smatch after_match;
	std::string remote_ip, requester, request_id, operation, key, request_uri;
	std::string http_status, error_code, bytes_sent, object_size;

	if (std::regex_search(after_ts, after_match, after_ts_pattern)) {
		remote_ip = after_match[1].str();
		requester = after_match[2].str();
		request_id = after_match[3].str();
		operation = after_match[4].str();
		key = after_match[5].str();
		request_uri = after_match[6].str();
		http_status = after_match[7].str();
		error_code = after_match[8].str();
		bytes_sent = after_match[9].str();
		object_size = after_match[10].str();
	} else {
		// Simpler fallback parsing
		std::istringstream after_stream(after_ts);
		after_stream >> remote_ip >> requester >> request_id >> operation >> key;
	}

	// Determine severity based on HTTP status
	int status_code = 0;
	try {
		status_code = std::stoi(http_status);
	} catch (...) {
	}

	if (status_code >= 400 && status_code < 500) {
		event.severity = "warning";
		event.status = ValidationEventStatus::WARNING;
	} else if (status_code >= 500) {
		event.severity = "error";
		event.status = ValidationEventStatus::ERROR;
	} else {
		event.severity = "info";
		event.status = ValidationEventStatus::INFO;
	}

	// Build message
	std::string msg = operation;
	if (!key.empty() && key != "-") {
		msg += " " + key;
	}
	if (!http_status.empty()) {
		msg += " -> " + http_status;
	}
	if (!error_code.empty() && error_code != "-") {
		msg += " (" + error_code + ")";
	}
	event.message = msg;

	// Map to schema fields
	event.origin = remote_ip;
	event.principal = requester;
	event.category = bucket;

	if (!error_code.empty() && error_code != "-") {
		event.error_code = error_code;
	}

	// Build structured_data JSON
	std::string json = "{";
	if (!bucket_owner.empty())
		json += "\"bucket_owner\":\"" + bucket_owner + "\"";
	if (!bucket.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"bucket\":\"" + bucket + "\"";
	}
	if (!remote_ip.empty() && remote_ip != "-") {
		if (json.length() > 1)
			json += ",";
		json += "\"remote_ip\":\"" + remote_ip + "\"";
	}
	if (!requester.empty() && requester != "-") {
		if (json.length() > 1)
			json += ",";
		// Escape any special chars in requester (ARNs have colons)
		std::string escaped;
		for (char c : requester) {
			if (c == '"')
				escaped += "\\\"";
			else if (c == '\\')
				escaped += "\\\\";
			else
				escaped += c;
		}
		json += "\"requester\":\"" + escaped + "\"";
	}
	if (!request_id.empty() && request_id != "-") {
		if (json.length() > 1)
			json += ",";
		json += "\"request_id\":\"" + request_id + "\"";
	}
	if (!operation.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"operation\":\"" + operation + "\"";
	}
	if (!key.empty() && key != "-") {
		if (json.length() > 1)
			json += ",";
		json += "\"key\":\"" + key + "\"";
	}
	if (!http_status.empty()) {
		if (json.length() > 1)
			json += ",";
		json += "\"http_status\":\"" + http_status + "\"";
	}
	if (!bytes_sent.empty() && bytes_sent != "-") {
		if (json.length() > 1)
			json += ",";
		json += "\"bytes_sent\":\"" + bytes_sent + "\"";
	}
	json += "}";
	event.structured_data = json;

	event.log_content = line;
	return true;
}

bool S3AccessParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	std::istringstream stream(content);
	std::string line;
	int s3_lines = 0;
	int checked = 0;

	// Pattern to detect S3 access logs
	// Look for the timestamp format and REST operation patterns
	static std::regex s3_detect(
	    R"(\[\d{2}/\w{3}/\d{4}:\d{2}:\d{2}:\d{2}\s+[+-]\d{4}\].*REST\.(GET|PUT|DELETE|HEAD|POST)\.)",
	    std::regex::optimize);

	while (std::getline(stream, line) && checked < 10) {
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);
		if (line.empty())
			continue;

		checked++;

		if (std::regex_search(line, s3_detect)) {
			s3_lines++;
		}
	}

	return s3_lines > 0 && s3_lines >= (checked / 3);
}

std::vector<ValidationEvent> S3AccessParser::parse(const std::string &content) const {
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

		if (line.empty())
			continue;

		ValidationEvent event;
		if (ParseS3AccessLine(line, event, event_id, line_number)) {
			events.push_back(event);
			event_id++;
		}
	}

	return events;
}

} // namespace duckdb
