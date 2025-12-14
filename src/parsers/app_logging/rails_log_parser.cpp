#include "rails_log_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

static ValidationEventStatus MapStatusCodeToStatus(int status_code) {
    if (status_code >= 500) return ValidationEventStatus::ERROR;
    if (status_code >= 400) return ValidationEventStatus::WARNING;
    return ValidationEventStatus::INFO;
}

static std::string MapStatusCodeToSeverity(int status_code) {
    if (status_code >= 500) return "error";
    if (status_code >= 400) return "warning";
    return "info";
}

// Rails log formats:
// "Started GET "/users" for 127.0.0.1 at 2025-01-15 10:30:45 +0000"
// "Processing by UsersController#index as HTML"
// "Completed 200 OK in 45ms (Views: 30.2ms | ActiveRecord: 12.1ms)"

struct RailsRequest {
    std::string method;
    std::string path;
    std::string remote_ip;
    std::string timestamp;
    std::string controller;
    std::string action;
    std::string format;
    int status_code;
    std::string duration;
    std::string views_time;
    std::string ar_time;
    int start_line;
    int end_line;
    std::string raw_output;
    bool has_started;
    bool has_completed;
};

static bool ParseRailsLine(const std::string& line, RailsRequest& request, int line_number) {
    // Started line
    static std::regex started_pattern(
        R"(^Started\s+(\w+)\s+\"([^\"]+)\"\s+for\s+(\S+)\s+at\s+(.+)$)",
        std::regex::optimize
    );

    // Processing line
    static std::regex processing_pattern(
        R"(^Processing\s+by\s+(\w+)#(\w+)\s+as\s+(\w+))",
        std::regex::optimize
    );

    // Completed line - status text can be multiple words (e.g., "Internal Server Error", "Not Found")
    static std::regex completed_pattern(
        R"(^Completed\s+(\d+)\s+.+?\s+in\s+(\d+(?:\.\d+)?ms)(?:\s+\(Views:\s+(\d+(?:\.\d+)?ms))?(?:\s*\|\s*ActiveRecord:\s+(\d+(?:\.\d+)?ms)\))?)",
        std::regex::optimize
    );

    std::smatch match;

    if (std::regex_match(line, match, started_pattern)) {
        request.method = match[1].str();
        request.path = match[2].str();
        request.remote_ip = match[3].str();
        request.timestamp = match[4].str();
        request.start_line = line_number;
        request.has_started = true;
        if (request.raw_output.empty()) {
            request.raw_output = line;
        } else {
            request.raw_output += "\n" + line;
        }
        return true;
    }

    if (std::regex_match(line, match, processing_pattern)) {
        request.controller = match[1].str();
        request.action = match[2].str();
        request.format = match[3].str();
        request.raw_output += "\n" + line;
        return true;
    }

    if (std::regex_match(line, match, completed_pattern)) {
        request.status_code = std::stoi(match[1].str());
        request.duration = match[2].str();
        if (match[3].matched) request.views_time = match[3].str();
        if (match[4].matched) request.ar_time = match[4].str();
        request.end_line = line_number;
        request.has_completed = true;
        request.raw_output += "\n" + line;
        return true;
    }

    return false;
}

static void CreateEventFromRequest(const RailsRequest& request, ValidationEvent& event, int64_t event_id) {
    event.event_id = event_id;
    event.tool_name = "rails_log";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = request.start_line;
    event.log_line_end = request.end_line > 0 ? request.end_line : request.start_line;
    event.line_number = -1;
    event.column_number = -1;

    // Parse duration to execution_time
    if (!request.duration.empty()) {
        try {
            std::string dur = request.duration;
            if (dur.back() == 's' && dur[dur.size()-2] == 'm') {
                dur = dur.substr(0, dur.size() - 2);
            }
            event.execution_time = std::stod(dur);
        } catch (...) {
            event.execution_time = 0.0;
        }
    }

    event.started_at = request.timestamp;
    event.origin = request.remote_ip;
    event.file_path = request.path;

    if (!request.controller.empty()) {
        event.category = request.controller + "#" + request.action;
    } else {
        event.category = request.method;
    }

    // Build message
    std::string msg = request.method + " " + request.path;
    if (request.status_code > 0) {
        msg += " -> " + std::to_string(request.status_code);
        if (!request.duration.empty()) {
            msg += " (" + request.duration + ")";
        }
    }
    event.message = msg;

    if (request.status_code > 0) {
        event.error_code = std::to_string(request.status_code);
        event.severity = MapStatusCodeToSeverity(request.status_code);
        event.status = MapStatusCodeToStatus(request.status_code);
    } else {
        event.severity = "info";
        event.status = ValidationEventStatus::INFO;
    }

    // Build structured_data JSON
    std::string json = "{";
    json += "\"method\":\"" + request.method + "\"";
    json += ",\"path\":\"" + request.path + "\"";
    if (!request.remote_ip.empty()) json += ",\"remote_ip\":\"" + request.remote_ip + "\"";
    if (!request.controller.empty()) json += ",\"controller\":\"" + request.controller + "\"";
    if (!request.action.empty()) json += ",\"action\":\"" + request.action + "\"";
    if (!request.format.empty()) json += ",\"format\":\"" + request.format + "\"";
    if (request.status_code > 0) json += ",\"status\":" + std::to_string(request.status_code);
    if (!request.duration.empty()) json += ",\"duration\":\"" + request.duration + "\"";
    if (!request.views_time.empty()) json += ",\"views_time\":\"" + request.views_time + "\"";
    if (!request.ar_time.empty()) json += ",\"ar_time\":\"" + request.ar_time + "\"";
    json += "}";
    event.structured_data = json;

    event.raw_output = request.raw_output;
}

bool RailsLogParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    std::istringstream stream(content);
    std::string line;
    int rails_lines = 0;
    int checked = 0;

    // Rails detection patterns
    static std::regex started_detect(R"(^Started\s+(GET|POST|PUT|PATCH|DELETE|HEAD|OPTIONS)\s+\")", std::regex::optimize);
    static std::regex processing_detect(R"(^Processing\s+by\s+\w+#\w+\s+as)", std::regex::optimize);
    static std::regex completed_detect(R"(^Completed\s+\d+\s+\w+\s+in\s+\d)", std::regex::optimize);

    while (std::getline(stream, line) && checked < 15) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, started_detect) ||
            std::regex_search(line, processing_detect) ||
            std::regex_search(line, completed_detect)) {
            rails_lines++;
        }
    }

    return rails_lines > 0 && rails_lines >= (checked / 4);
}

std::vector<ValidationEvent> RailsLogParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int line_number = 0;

    RailsRequest current_request;
    current_request.status_code = 0;
    current_request.start_line = 0;
    current_request.end_line = 0;
    current_request.has_started = false;
    current_request.has_completed = false;

    auto finalize_request = [&]() {
        if (current_request.has_started) {
            ValidationEvent event;
            CreateEventFromRequest(current_request, event, event_id);
            events.push_back(event);
            event_id++;
        }
        current_request = RailsRequest();
        current_request.status_code = 0;
        current_request.start_line = 0;
        current_request.end_line = 0;
        current_request.has_started = false;
        current_request.has_completed = false;
    };

    while (std::getline(stream, line)) {
        line_number++;

        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
            line = line.substr(0, end + 1);
        }

        if (line.empty()) continue;

        // Check for new request start
        if (line.find("Started ") == 0) {
            // Finalize previous request if any
            finalize_request();
        }

        if (ParseRailsLine(line, current_request, line_number)) {
            // If completed, finalize this request
            if (current_request.has_completed) {
                finalize_request();
            }
        }
    }

    // Finalize any remaining request
    finalize_request();

    return events;
}

} // namespace duckdb
