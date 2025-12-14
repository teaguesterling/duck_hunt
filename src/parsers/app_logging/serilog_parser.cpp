#include "serilog_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include "yyjson.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

using namespace duckdb_yyjson;

// Serilog levels
static std::string MapSerilogLevel(const std::string& level) {
    std::string lower = level;
    for (char& c : lower) c = std::tolower(c);

    if (lower == "fatal" || lower == "error" || lower == "ftl" || lower == "err") {
        return "error";
    } else if (lower == "warning" || lower == "wrn") {
        return "warning";
    }
    return "info"; // Information, Debug, Verbose, INF, DBG, VRB
}

static ValidationEventStatus MapLevelToStatus(const std::string& severity) {
    if (severity == "error") return ValidationEventStatus::ERROR;
    if (severity == "warning") return ValidationEventStatus::WARNING;
    return ValidationEventStatus::INFO;
}

// Parse Serilog JSON (Compact) format:
// {"@t":"2025-01-15T10:30:45.123Z","@mt":"User {UserId} logged in","UserId":123,"@l":"Information"}
static bool ParseSerilogJSON(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    yyjson_doc* doc = yyjson_read(line.c_str(), line.length(), 0);
    if (!doc) return false;

    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    // Serilog Compact uses @t (timestamp) and @mt (message template)
    yyjson_val* t_val = yyjson_obj_get(root, "@t");
    yyjson_val* mt_val = yyjson_obj_get(root, "@mt");

    // Must have @t or @mt to be Serilog Compact format
    if (!t_val && !mt_val) {
        yyjson_doc_free(doc);
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "serilog";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    if (t_val && yyjson_is_str(t_val)) {
        event.started_at = yyjson_get_str(t_val);
    }

    if (mt_val && yyjson_is_str(mt_val)) {
        event.message = yyjson_get_str(mt_val);
    }

    yyjson_val* l_val = yyjson_obj_get(root, "@l");
    std::string level = l_val && yyjson_is_str(l_val) ? yyjson_get_str(l_val) : "Information";
    event.severity = MapSerilogLevel(level);
    event.status = MapLevelToStatus(event.severity);

    // Check for exception
    yyjson_val* x_val = yyjson_obj_get(root, "@x");
    if (x_val && yyjson_is_str(x_val)) {
        event.error_code = yyjson_get_str(x_val);
    }

    // SourceContext as category
    yyjson_val* sc_val = yyjson_obj_get(root, "SourceContext");
    if (sc_val && yyjson_is_str(sc_val)) {
        event.category = yyjson_get_str(sc_val);
    }

    event.structured_data = line;
    event.raw_output = line;

    yyjson_doc_free(doc);
    return true;
}

// Parse Serilog text format:
// "[10:30:45 INF] User 123 logged in"
// "[2025-01-15 10:30:45.123 +00:00] [INF] User logged in"
static bool ParseSerilogText(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    // Pattern 1: [HH:MM:SS LVL] message
    static std::regex serilog_short_pattern(
        R"(^\[(\d{2}:\d{2}:\d{2})\s+(VRB|DBG|INF|WRN|ERR|FTL)\]\s*(.*)$)",
        std::regex::optimize
    );

    // Pattern 2: [timestamp] [LVL] message
    static std::regex serilog_full_pattern(
        R"(^\[(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:\s+[+-]\d{2}:\d{2})?)\]\s+\[(VRB|DBG|INF|WRN|ERR|FTL)\]\s*(.*)$)",
        std::regex::optimize
    );

    std::smatch match;

    if (std::regex_match(line, match, serilog_short_pattern)) {
        event.event_id = event_id;
        event.tool_name = "serilog";
        event.event_type = ValidationEventType::DEBUG_INFO;
        event.log_line_start = line_number;
        event.log_line_end = line_number;
        event.execution_time = 0.0;
        event.line_number = -1;
        event.column_number = -1;

        event.started_at = match[1].str();
        std::string level = match[2].str();
        event.message = match[3].str();

        event.severity = MapSerilogLevel(level);
        event.status = MapLevelToStatus(event.severity);

        std::string json = "{\"level\":\"" + level + "\"}";
        event.structured_data = json;
        event.raw_output = line;
        return true;
    }

    if (std::regex_match(line, match, serilog_full_pattern)) {
        event.event_id = event_id;
        event.tool_name = "serilog";
        event.event_type = ValidationEventType::DEBUG_INFO;
        event.log_line_start = line_number;
        event.log_line_end = line_number;
        event.execution_time = 0.0;
        event.line_number = -1;
        event.column_number = -1;

        event.started_at = match[1].str();
        std::string level = match[2].str();
        event.message = match[3].str();

        event.severity = MapSerilogLevel(level);
        event.status = MapLevelToStatus(event.severity);

        std::string json = "{\"level\":\"" + level + "\"}";
        event.structured_data = json;
        event.raw_output = line;
        return true;
    }

    return false;
}

bool SerilogParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    std::istringstream stream(content);
    std::string line;
    int serilog_lines = 0;
    int checked = 0;

    // Serilog JSON detection: has "@t" or "@mt"
    static std::regex json_detect(R"("@t"\s*:|"@mt"\s*:)", std::regex::optimize);
    // Serilog text detection: [time LVL] or [timestamp] [LVL]
    static std::regex text_detect(R"(^\[\d{2}:\d{2}:\d{2}\s+(VRB|DBG|INF|WRN|ERR|FTL)\]|^\[.*\]\s+\[(VRB|DBG|INF|WRN|ERR|FTL)\])", std::regex::optimize);

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, json_detect) || std::regex_search(line, text_detect)) {
            serilog_lines++;
        }
    }

    return serilog_lines > 0 && serilog_lines >= (checked / 3);
}

std::vector<ValidationEvent> SerilogParser::parse(const std::string& content) const {
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
        if (ParseSerilogJSON(line, event, event_id, line_number) ||
            ParseSerilogText(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
