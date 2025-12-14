#include "winston_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include "yyjson.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

using namespace duckdb_yyjson;

// Winston log levels
static std::string MapWinstonLevel(const std::string& level) {
    std::string lower = level;
    for (char& c : lower) c = std::tolower(c);

    if (lower == "error" || lower == "emerg" || lower == "alert" || lower == "crit") {
        return "error";
    } else if (lower == "warn" || lower == "warning") {
        return "warning";
    }
    return "info"; // info, http, verbose, debug, silly
}

static ValidationEventStatus MapLevelToStatus(const std::string& severity) {
    if (severity == "error") {
        return ValidationEventStatus::ERROR;
    } else if (severity == "warning") {
        return ValidationEventStatus::WARNING;
    }
    return ValidationEventStatus::INFO;
}

// Parse Winston JSON format:
// {"level":"error","message":"Connection timeout","service":"api","timestamp":"2025-01-15T10:30:45.123Z"}
static bool ParseWinstonJSON(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    yyjson_doc* doc = yyjson_read(line.c_str(), line.length(), 0);
    if (!doc) return false;

    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    // Must have level or message to be winston
    yyjson_val* level_val = yyjson_obj_get(root, "level");
    yyjson_val* message_val = yyjson_obj_get(root, "message");

    if (!level_val && !message_val) {
        yyjson_doc_free(doc);
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "winston";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    std::string level = level_val && yyjson_is_str(level_val) ? yyjson_get_str(level_val) : "info";
    event.severity = MapWinstonLevel(level);
    event.status = MapLevelToStatus(event.severity);

    if (message_val && yyjson_is_str(message_val)) {
        event.message = yyjson_get_str(message_val);
    }

    yyjson_val* timestamp_val = yyjson_obj_get(root, "timestamp");
    if (timestamp_val && yyjson_is_str(timestamp_val)) {
        event.started_at = yyjson_get_str(timestamp_val);
    }

    yyjson_val* service_val = yyjson_obj_get(root, "service");
    if (service_val && yyjson_is_str(service_val)) {
        event.category = yyjson_get_str(service_val);
    }

    // Store full JSON as structured data
    event.structured_data = line;
    event.raw_output = line;

    yyjson_doc_free(doc);
    return true;
}

// Parse Winston text format:
// "2025-01-15T10:30:45.123Z [api] error: Connection timeout"
static bool ParseWinstonText(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Pattern: timestamp [service] level: message
    static std::regex winston_text_pattern(
        R"(^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z)\s+\[([^\]]+)\]\s+(\w+):\s*(.*)$)",
        std::regex::optimize
    );

    std::smatch match;
    if (!std::regex_match(line, match, winston_text_pattern)) {
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "winston";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    event.started_at = match[1].str();
    event.category = match[2].str();
    std::string level = match[3].str();
    event.message = match[4].str();

    event.severity = MapWinstonLevel(level);
    event.status = MapLevelToStatus(event.severity);

    // Build structured_data JSON
    std::string json = "{";
    json += "\"level\":\"" + level + "\"";
    json += ",\"service\":\"" + event.category + "\"";
    json += "}";
    event.structured_data = json;

    event.raw_output = line;
    return true;
}

bool WinstonParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    std::istringstream stream(content);
    std::string line;
    int winston_lines = 0;
    int checked = 0;

    // Winston JSON detection: has "level" and often "message"
    static std::regex json_detect(R"RE("level"\s*:\s*"(error|warn|info|http|verbose|debug|silly)")RE", std::regex::optimize | std::regex::icase);
    // Winston text detection
    static std::regex text_detect(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z\s+\[)", std::regex::optimize);

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, json_detect) || std::regex_search(line, text_detect)) {
            winston_lines++;
        }
    }

    return winston_lines > 0 && winston_lines >= (checked / 3);
}

std::vector<ValidationEvent> WinstonParser::parse(const std::string& content) const {
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
        // Try JSON first, then text
        if (ParseWinstonJSON(line, event, event_id, line_number) ||
            ParseWinstonText(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
