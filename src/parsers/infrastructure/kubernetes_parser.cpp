#include "kubernetes_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Kubernetes log formats:
// 1. klog format (used by kubelet, controller-manager, etc):
//    "I0115 10:30:45.123456    1234 controller.go:123] Message here"
//    "E0115 10:30:45.123456    1234 controller.go:123] Error message"
//
// 2. kubectl logs output:
//    "2025-01-15T10:30:45.123456789Z stdout F Normal log message"
//    Or just the raw container output
//
// 3. Kubernetes events:
//    "LAST SEEN   TYPE     REASON    OBJECT          MESSAGE"
//    "5m          Normal   Pulled    pod/nginx-abc   Successfully pulled image"

static std::string MapKlogLevel(char level) {
	switch (level) {
	case 'E':
		return "error";
	case 'W':
		return "warning";
	case 'I':
		return "info";
	case 'D':
		return "info"; // debug
	default:
		return "info";
	}
}

static ValidationEventStatus MapLevelToStatus(const std::string &severity) {
	if (severity == "error") {
		return ValidationEventStatus::ERROR;
	} else if (severity == "warning") {
		return ValidationEventStatus::WARNING;
	}
	return ValidationEventStatus::INFO;
}

static bool ParseKubernetesLine(const std::string &line, ValidationEvent &event, int64_t event_id, int line_number) {
	// klog format: "I0115 10:30:45.123456    1234 file.go:123] message"
	static std::regex klog_pattern(R"(^([IWED])(\d{4})\s+(\d{2}:\d{2}:\d{2}\.\d+)\s+(\d+)\s+(\S+):(\d+)\]\s*(.*)$)",
	                               std::regex::optimize);

	// kubectl logs format with timestamp: "2025-01-15T10:30:45.123456789Z stdout F message"
	static std::regex kubectl_pattern(
	    R"(^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z)\s+(stdout|stderr)\s+([FP])\s*(.*)$)", std::regex::optimize);

	// Kubernetes events format (from kubectl get events)
	static std::regex events_pattern(R"(^(\S+)\s+(Normal|Warning)\s+(\S+)\s+(\S+)\s+(.*)$)",
	                                 std::regex::optimize | std::regex::icase);

	std::smatch match;

	// Try klog format
	if (std::regex_match(line, match, klog_pattern)) {
		event.event_id = event_id;
		event.tool_name = "kubernetes";
		event.event_type = ValidationEventType::DEBUG_INFO;
		event.log_line_start = line_number;
		event.log_line_end = line_number;
		event.execution_time = 0.0;

		char level_char = match[1].str()[0];
		std::string date_part = match[2].str(); // MMDD
		std::string time_part = match[3].str();
		std::string pid = match[4].str();
		std::string file = match[5].str();
		std::string line_num_str = match[6].str();
		std::string message = match[7].str();

		event.severity = MapKlogLevel(level_char);
		event.status = MapLevelToStatus(event.severity);

		// Parse line number
		try {
			event.ref_line = std::stoi(line_num_str);
		} catch (...) {
			event.ref_line = -1;
		}
		event.ref_column = -1;

		event.ref_file = file;
		event.message = message;
		event.started_at = time_part;
		event.category = file;

		// Build structured_data JSON
		std::string json = "{";
		json += "\"level\":\"" + std::string(1, level_char) + "\"";
		json += ",\"file\":\"" + file + "\"";
		json += ",\"line\":" + line_num_str;
		json += ",\"pid\":\"" + pid + "\"";
		json += "}";
		event.structured_data = json;

		event.log_content = line;
		return true;
	}

	// Try kubectl logs format
	if (std::regex_match(line, match, kubectl_pattern)) {
		event.event_id = event_id;
		event.tool_name = "kubernetes";
		event.event_type = ValidationEventType::DEBUG_INFO;
		event.log_line_start = line_number;
		event.log_line_end = line_number;
		event.execution_time = 0.0;
		event.ref_line = -1;
		event.ref_column = -1;

		std::string timestamp = match[1].str();
		std::string stream = match[2].str();
		std::string partial = match[3].str();
		std::string message = match[4].str();

		// stderr often indicates errors
		if (stream == "stderr") {
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
		} else {
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
		}

		event.started_at = timestamp;
		event.message = message;
		event.category = stream;

		// Build structured_data JSON
		std::string json = "{";
		json += "\"stream\":\"" + stream + "\"";
		json += ",\"partial\":\"" + partial + "\"";
		json += "}";
		event.structured_data = json;

		event.log_content = line;
		return true;
	}

	// Try Kubernetes events format
	if (std::regex_match(line, match, events_pattern)) {
		event.event_id = event_id;
		event.tool_name = "kubernetes";
		event.event_type = ValidationEventType::DEBUG_INFO;
		event.log_line_start = line_number;
		event.log_line_end = line_number;
		event.execution_time = 0.0;
		event.ref_line = -1;
		event.ref_column = -1;

		std::string last_seen = match[1].str();
		std::string event_type = match[2].str();
		std::string reason = match[3].str();
		std::string object = match[4].str();
		std::string message = match[5].str();

		// Normalize event type
		for (char &c : event_type)
			c = std::tolower(c);

		if (event_type == "warning") {
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
		} else {
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
		}

		event.message = reason + ": " + message;
		event.category = object;
		event.error_code = reason;

		// Build structured_data JSON
		std::string json = "{";
		json += "\"last_seen\":\"" + last_seen + "\"";
		json += ",\"type\":\"" + event_type + "\"";
		json += ",\"reason\":\"" + reason + "\"";
		json += ",\"object\":\"" + object + "\"";
		json += "}";
		event.structured_data = json;

		event.log_content = line;
		return true;
	}

	return false;
}

bool KubernetesParser::canParse(const std::string &content) const {
	if (content.empty()) {
		return false;
	}

	std::istringstream stream(content);
	std::string line;
	int k8s_lines = 0;
	int checked = 0;

	// Patterns to detect Kubernetes logs
	static std::regex klog_detect(R"(^[IWED]\d{4}\s+\d{2}:\d{2}:\d{2}\.\d+)", std::regex::optimize);
	static std::regex kubectl_detect(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z\s+(stdout|stderr))",
	                                 std::regex::optimize);
	static std::regex events_detect(R"(^\S+\s+(Normal|Warning)\s+\S+\s+\S+/)",
	                                std::regex::optimize | std::regex::icase);

	while (std::getline(stream, line) && checked < 10) {
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			continue;
		line = line.substr(start);
		if (line.empty())
			continue;

		checked++;

		if (std::regex_search(line, klog_detect) || std::regex_search(line, kubectl_detect) ||
		    std::regex_search(line, events_detect)) {
			k8s_lines++;
		}
	}

	return k8s_lines > 0 && k8s_lines >= (checked / 3);
}

std::vector<ValidationEvent> KubernetesParser::parse(const std::string &content) const {
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

		// Skip header lines
		if (line.find("LAST SEEN") != std::string::npos && line.find("TYPE") != std::string::npos) {
			continue;
		}

		ValidationEvent event;
		if (ParseKubernetesLine(line, event, event_id, line_number)) {
			events.push_back(event);
			event_id++;
		}
	}

	return events;
}

} // namespace duckdb
