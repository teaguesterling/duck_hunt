#include "bunyan_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include "yyjson.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

using namespace duckdb_yyjson;

// Bunyan log levels (same as Pino):
// 10=trace, 20=debug, 30=info, 40=warn, 50=error, 60=fatal
static std::string MapBunyanLevel(int level) {
    if (level >= 50) return "error";
    if (level >= 40) return "warning";
    return "info";
}

static ValidationEventStatus MapLevelToStatus(const std::string& severity) {
    if (severity == "error") return ValidationEventStatus::ERROR;
    if (severity == "warning") return ValidationEventStatus::WARNING;
    return ValidationEventStatus::INFO;
}

// Parse Bunyan JSON format:
// {"name":"myapp","hostname":"server1","pid":1234,"level":30,"msg":"listening","time":"2025-01-15T10:30:45.123Z","v":0}
static bool ParseBunyanLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    yyjson_doc* doc = yyjson_read(line.c_str(), line.length(), 0);
    if (!doc) return false;

    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    // Bunyan has "v" (version) field and "name"
    yyjson_val* v_val = yyjson_obj_get(root, "v");
    yyjson_val* name_val = yyjson_obj_get(root, "name");
    yyjson_val* level_val = yyjson_obj_get(root, "level");

    // Must have "v" field or ("name" and numeric "level") to be Bunyan
    bool is_bunyan = (v_val && yyjson_is_int(v_val)) ||
                     (name_val && level_val && yyjson_is_int(level_val));
    if (!is_bunyan) {
        yyjson_doc_free(doc);
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "bunyan";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.line_number = -1;
    event.column_number = -1;

    int level = level_val && yyjson_is_int(level_val) ? yyjson_get_int(level_val) : 30;
    event.severity = MapBunyanLevel(level);
    event.status = MapLevelToStatus(event.severity);

    yyjson_val* time_val = yyjson_obj_get(root, "time");
    if (time_val && yyjson_is_str(time_val)) {
        event.started_at = yyjson_get_str(time_val);
    }

    yyjson_val* msg_val = yyjson_obj_get(root, "msg");
    if (msg_val && yyjson_is_str(msg_val)) {
        event.message = yyjson_get_str(msg_val);
    }

    if (name_val && yyjson_is_str(name_val)) {
        event.category = yyjson_get_str(name_val);
    }

    yyjson_val* hostname_val = yyjson_obj_get(root, "hostname");
    if (hostname_val && yyjson_is_str(hostname_val)) {
        event.origin = yyjson_get_str(hostname_val);
    }

    // Check for error
    yyjson_val* err_val = yyjson_obj_get(root, "err");
    if (err_val && yyjson_is_obj(err_val)) {
        yyjson_val* err_msg = yyjson_obj_get(err_val, "message");
        if (err_msg && yyjson_is_str(err_msg)) {
            event.error_code = yyjson_get_str(err_msg);
        }
    }

    event.structured_data = line;
    event.raw_output = line;

    yyjson_doc_free(doc);
    return true;
}

bool BunyanParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    std::istringstream stream(content);
    std::string line;
    int bunyan_lines = 0;
    int checked = 0;

    // Bunyan detection: has "v" field (version) and "name"
    static std::regex bunyan_detect(R"("v"\s*:\s*\d+.*"name"\s*:|"name"\s*:.*"v"\s*:\s*\d+)", std::regex::optimize);

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, bunyan_detect)) {
            bunyan_lines++;
        }
    }

    return bunyan_lines > 0 && bunyan_lines >= (checked / 3);
}

std::vector<ValidationEvent> BunyanParser::parse(const std::string& content) const {
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
        if (ParseBunyanLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
