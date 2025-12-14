#include "logrus_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>
#include <unordered_map>

namespace duckdb {

// Map Go log levels to our severity
static std::string MapGoLevel(const std::string& level) {
    // Logrus levels: panic, fatal, error, warn/warning, info, debug, trace
    if (level == "panic" || level == "fatal" || level == "error") {
        return "error";
    } else if (level == "warn" || level == "warning") {
        return "warning";
    }
    return "info";  // info, debug, trace
}

static ValidationEventStatus MapLevelToStatus(const std::string& severity) {
    if (severity == "error") {
        return ValidationEventStatus::ERROR;
    } else if (severity == "warning") {
        return ValidationEventStatus::WARNING;
    }
    return ValidationEventStatus::INFO;
}

// Parse key=value pairs from logrus text format
static std::unordered_map<std::string, std::string> ParseKeyValuePairs(const std::string& line) {
    std::unordered_map<std::string, std::string> pairs;

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
        pairs[key] = value;
    }

    return pairs;
}

// Parse Logrus text format:
// time="2025-01-15T10:30:45Z" level=info msg="server started" port=8080
// Also handles colored output format (after stripping ANSI):
// INFO[0000] server started                               port=8080
static bool ParseLogrusLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Key-value format (most common)
    if (line.find("time=") != std::string::npos || line.find("level=") != std::string::npos) {
        auto pairs = ParseKeyValuePairs(line);

        if (pairs.find("level") == pairs.end() && pairs.find("msg") == pairs.end()) {
            return false;  // Not a valid logrus line
        }

        event.event_id = event_id;
        event.tool_name = "logrus";
        event.event_type = ValidationEventType::DEBUG_INFO;
        event.log_line_start = line_number;
        event.log_line_end = line_number;
        event.execution_time = 0.0;
        event.line_number = -1;
        event.column_number = -1;

        // Extract standard fields
        std::string level = pairs.count("level") ? pairs["level"] : "info";
        event.started_at = pairs.count("time") ? pairs["time"] : "";
        event.message = pairs.count("msg") ? pairs["msg"] : "";
        event.severity = MapGoLevel(level);
        event.status = MapLevelToStatus(event.severity);

        // Extract caller info if present
        if (pairs.count("file")) {
            event.file_path = pairs["file"];
        }
        if (pairs.count("func")) {
            event.function_name = pairs["func"];
        }

        // Look for error field
        if (pairs.count("error")) {
            event.error_code = pairs["error"];
        }

        // Build structured_data JSON with all fields
        std::string json = "{";
        bool first = true;
        for (const auto& pair : pairs) {
            if (!first) json += ",";
            first = false;
            // Escape value
            std::string escaped;
            for (char c : pair.second) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else escaped += c;
            }
            json += "\"" + pair.first + "\":\"" + escaped + "\"";
        }
        json += "}";
        event.structured_data = json;

        event.raw_output = line;
        return true;
    }

    // Colored/console format: LEVEL[time] message key=value key=value
    // e.g., INFO[0000] Starting server                            port=8080
    static std::regex console_pattern(
        R"(^(PANIC|FATAL|ERROR|WARN|WARNING|INFO|DEBUG|TRACE)\[(\d+)\]\s+(.+?)(?:\s{2,}(.*))?$)",
        std::regex::optimize | std::regex::icase
    );

    std::smatch match;
    if (std::regex_match(line, match, console_pattern)) {
        event.event_id = event_id;
        event.tool_name = "logrus";
        event.event_type = ValidationEventType::DEBUG_INFO;
        event.log_line_start = line_number;
        event.log_line_end = line_number;
        event.execution_time = 0.0;
        event.line_number = -1;
        event.column_number = -1;

        std::string level = match[1].str();
        // Normalize to lowercase
        for (char& c : level) c = std::tolower(c);

        event.message = match[3].str();
        // Trim trailing spaces from message
        size_t msg_end = event.message.find_last_not_of(" \t");
        if (msg_end != std::string::npos) {
            event.message = event.message.substr(0, msg_end + 1);
        }

        event.severity = MapGoLevel(level);
        event.status = MapLevelToStatus(event.severity);

        // Parse additional key=value pairs if present
        std::string extra = match[4].matched ? match[4].str() : "";
        auto pairs = ParseKeyValuePairs(extra);

        // Build structured_data JSON
        std::string json = "{";
        json += "\"level\":\"" + level + "\"";
        json += ",\"elapsed\":\"" + match[2].str() + "\"";
        for (const auto& pair : pairs) {
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

    return false;
}

bool LogrusParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    int logrus_lines = 0;
    int checked = 0;

    // Patterns to detect Logrus-style logging
    static std::regex kv_detect_pattern(
        R"(time=|level=|msg=)",
        std::regex::optimize
    );
    static std::regex console_detect_pattern(
        R"(^(PANIC|FATAL|ERROR|WARN|WARNING|INFO|DEBUG|TRACE)\[\d+\])",
        std::regex::optimize | std::regex::icase
    );

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, kv_detect_pattern) ||
            std::regex_search(line, console_detect_pattern)) {
            logrus_lines++;
        }
    }

    return logrus_lines > 0 && logrus_lines >= (checked / 3);
}

std::vector<ValidationEvent> LogrusParser::parse(const std::string& content) const {
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

        // Strip ANSI escape codes if present
        static std::regex ansi_pattern(R"(\x1b\[[0-9;]*m)", std::regex::optimize);
        line = std::regex_replace(line, ansi_pattern, "");

        ValidationEvent event;
        if (ParseLogrusLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
