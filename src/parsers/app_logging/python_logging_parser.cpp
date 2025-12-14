#include "python_logging_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Map Python log levels to our severity
static std::string MapPythonLevel(const std::string& level) {
    // Python levels: DEBUG, INFO, WARNING, ERROR, CRITICAL
    if (level == "ERROR" || level == "CRITICAL" || level == "FATAL") {
        return "error";
    } else if (level == "WARNING" || level == "WARN") {
        return "warning";
    }
    return "info";  // DEBUG, INFO, NOTSET
}

static ValidationEventStatus MapLevelToStatus(const std::string& severity) {
    if (severity == "error") {
        return ValidationEventStatus::ERROR;
    } else if (severity == "warning") {
        return ValidationEventStatus::WARNING;
    }
    return ValidationEventStatus::INFO;
}

// Parse Python stdlib logging format:
// "2025-01-15 10:30:45,123 - myapp.module - INFO - User login successful"
// Also handles variations like:
// "2025-01-15 10:30:45,123 INFO myapp.module - Message"
// "INFO:myapp.module:Message"
static bool ParsePythonLogLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Standard format: timestamp - logger - level - message
    static std::regex standard_pattern(
        R"(^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}[,\.]\d+)\s+-\s+(\S+)\s+-\s+(DEBUG|INFO|WARNING|WARN|ERROR|CRITICAL|FATAL)\s+-\s+(.*)$)",
        std::regex::optimize | std::regex::icase
    );

    // Alternative format: timestamp level logger - message
    static std::regex alt_pattern(
        R"(^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}[,\.]\d+)\s+(DEBUG|INFO|WARNING|WARN|ERROR|CRITICAL|FATAL)\s+(\S+)\s+-?\s*(.*)$)",
        std::regex::optimize | std::regex::icase
    );

    // Simple format: LEVEL:logger:message (common in basicConfig)
    static std::regex simple_pattern(
        R"(^(DEBUG|INFO|WARNING|WARN|ERROR|CRITICAL|FATAL):(\S+):(.*)$)",
        std::regex::optimize | std::regex::icase
    );

    // Format with file/line: timestamp - logger - level - message (file:line)
    // Or: 2025-01-15 10:30:45,123 - myapp - ERROR - Error occurred
    //     File "/path/to/file.py", line 42, in function_name
    // Note: file_line_pattern not used in this function, moved to parse()

    std::smatch match;
    std::string timestamp;
    std::string logger;
    std::string level;
    std::string message;

    if (std::regex_match(line, match, standard_pattern)) {
        timestamp = match[1].str();
        logger = match[2].str();
        level = match[3].str();
        message = match[4].str();
    } else if (std::regex_match(line, match, alt_pattern)) {
        timestamp = match[1].str();
        level = match[2].str();
        logger = match[3].str();
        message = match[4].str();
    } else if (std::regex_match(line, match, simple_pattern)) {
        level = match[1].str();
        logger = match[2].str();
        message = match[3].str();
    } else {
        return false;
    }

    // Normalize level to uppercase for mapping
    std::string upper_level;
    for (char c : level) {
        upper_level += std::toupper(c);
    }

    event.event_id = event_id;
    event.tool_name = "python_logging";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    // Field mappings
    event.started_at = timestamp;
    event.category = logger;
    event.message = message;
    event.severity = MapPythonLevel(upper_level);
    event.status = MapLevelToStatus(event.severity);

    // Build structured_data JSON
    std::string json = "{";
    json += "\"logger\":\"" + logger + "\"";
    json += ",\"level\":\"" + upper_level + "\"";
    json += "}";
    event.structured_data = json;

    event.raw_output = line;
    return true;
}

// Check for Python traceback continuation lines
static bool IsTracebackLine(const std::string& line) {
    if (line.empty()) return false;

    // Traceback header
    if (line.find("Traceback (most recent call last)") != std::string::npos) {
        return true;
    }

    // File reference line
    if (line.find("  File \"") == 0) {
        return true;
    }

    // Code line (indented)
    if (line.size() >= 4 && line.substr(0, 4) == "    " &&
        line.find("  File \"") == std::string::npos) {
        return true;
    }

    // Exception line (e.g., "ValueError: invalid literal")
    static std::regex exception_pattern(R"(^[A-Z][a-zA-Z]*Error:|^[A-Z][a-zA-Z]*Exception:|^[A-Z][a-zA-Z]*Warning:)");
    if (std::regex_search(line, exception_pattern)) {
        return true;
    }

    return false;
}

bool PythonLoggingParser::canParse(const std::string& content) const {
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    int python_log_lines = 0;
    int checked = 0;

    // Patterns to detect Python logging
    static std::regex timestamp_logger_pattern(
        R"(^\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}[,\.]\d+\s+-?\s*\S+)",
        std::regex::optimize
    );
    static std::regex simple_level_pattern(
        R"(^(DEBUG|INFO|WARNING|WARN|ERROR|CRITICAL|FATAL):)",
        std::regex::optimize | std::regex::icase
    );

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, timestamp_logger_pattern) ||
            std::regex_search(line, simple_level_pattern)) {
            python_log_lines++;
        }
    }

    // Need at least some Python-style log lines
    return python_log_lines > 0 && python_log_lines >= (checked / 3);
}

std::vector<ValidationEvent> PythonLoggingParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int line_number = 0;

    ValidationEvent* current_event = nullptr;
    bool in_traceback = false;

    while (std::getline(stream, line)) {
        line_number++;

        // Trim trailing whitespace
        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
            line = line.substr(0, end + 1);
        }

        if (line.empty()) {
            in_traceback = false;
            continue;
        }

        // Check if this is a traceback continuation
        if (IsTracebackLine(line)) {
            if (current_event != nullptr) {
                current_event->raw_output += "\n" + line;
                current_event->log_line_end = line_number;

                // Try to extract file/line info from traceback
                static std::regex file_line_pattern(
                    R"re(^\s*File\s+"([^"]+)",\s+line\s+(\d+)(?:,\s+in\s+(\S+))?)re",
                    std::regex::optimize
                );
                std::smatch match;
                if (std::regex_search(line, match, file_line_pattern)) {
                    current_event->file_path = match[1].str();
                    try {
                        current_event->line_number = std::stoi(match[2].str());
                    } catch (...) {}
                    if (match[3].matched) {
                        current_event->function_name = match[3].str();
                    }
                }

                // Check for exception type
                static std::regex exception_pattern(R"(^([A-Z][a-zA-Z]*(?:Error|Exception|Warning)):\s*(.*)$)");
                if (std::regex_match(line, match, exception_pattern)) {
                    current_event->error_code = match[1].str();
                    if (!match[2].str().empty()) {
                        current_event->message = match[2].str();
                    }
                }
            }
            in_traceback = true;
            continue;
        }

        // Try to parse as a new log entry
        ValidationEvent event;
        if (ParsePythonLogLine(line, event, event_id, line_number)) {
            events.push_back(event);
            current_event = &events.back();
            event_id++;
            in_traceback = false;
        } else if (current_event != nullptr && !in_traceback) {
            // Continuation of previous message
            current_event->raw_output += "\n" + line;
            current_event->message += " " + line;
            current_event->log_line_end = line_number;
        }
    }

    return events;
}

} // namespace duckdb
