#include "pino_parser.hpp"
#include "duckdb/common/string_util.hpp"
#include "yyjson.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

using namespace duckdb_yyjson;

// Pino log levels (numeric):
// 10=trace, 20=debug, 30=info, 40=warn, 50=error, 60=fatal
static std::string MapPinoLevel(int level) {
    if (level >= 50) return "error";      // error, fatal
    if (level >= 40) return "warning";    // warn
    return "info";                         // trace, debug, info
}

static ValidationEventStatus MapLevelToStatus(const std::string& severity) {
    if (severity == "error") return ValidationEventStatus::ERROR;
    if (severity == "warning") return ValidationEventStatus::WARNING;
    return ValidationEventStatus::INFO;
}

// Parse Pino JSON format:
// {"level":30,"time":1705315845123,"pid":1234,"hostname":"server1","msg":"request completed"}
static bool ParsePinoLine(const std::string& line, ValidationEvent& event, int64_t event_id, int line_number) {
    yyjson_doc* doc = yyjson_read(line.c_str(), line.length(), 0);
    if (!doc) return false;

    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    // Pino uses numeric "level" and "time" (epoch ms)
    yyjson_val* level_val = yyjson_obj_get(root, "level");
    yyjson_val* time_val = yyjson_obj_get(root, "time");

    // Must have numeric level to be Pino
    if (!level_val || !yyjson_is_int(level_val)) {
        yyjson_doc_free(doc);
        return false;
    }

    event.event_id = event_id;
    event.tool_name = "pino";
    event.event_type = ValidationEventType::DEBUG_INFO;
    event.log_line_start = line_number;
    event.log_line_end = line_number;
    event.execution_time = 0.0;
    event.ref_line = -1;
    event.ref_column = -1;

    int level = yyjson_get_int(level_val);
    event.severity = MapPinoLevel(level);
    event.status = MapLevelToStatus(event.severity);

    // Convert epoch milliseconds to ISO timestamp
    if (time_val && yyjson_is_int(time_val)) {
        int64_t time_ms = yyjson_get_sint(time_val);
        time_t time_sec = time_ms / 1000;
        char buf[64];
        struct tm* tm_info = gmtime(&time_sec);
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm_info);
        event.started_at = std::string(buf) + "." + std::to_string(time_ms % 1000) + "Z";
    }

    yyjson_val* msg_val = yyjson_obj_get(root, "msg");
    if (msg_val && yyjson_is_str(msg_val)) {
        event.message = yyjson_get_str(msg_val);
    }

    yyjson_val* hostname_val = yyjson_obj_get(root, "hostname");
    if (hostname_val && yyjson_is_str(hostname_val)) {
        event.origin = yyjson_get_str(hostname_val);
    }

    yyjson_val* name_val = yyjson_obj_get(root, "name");
    if (name_val && yyjson_is_str(name_val)) {
        event.category = yyjson_get_str(name_val);
    }

    // Check for error object
    yyjson_val* err_val = yyjson_obj_get(root, "err");
    if (err_val && yyjson_is_obj(err_val)) {
        yyjson_val* err_msg = yyjson_obj_get(err_val, "message");
        if (err_msg && yyjson_is_str(err_msg)) {
            event.error_code = yyjson_get_str(err_msg);
        }
    }

    event.structured_data = line;
    event.log_content = line;

    yyjson_doc_free(doc);
    return true;
}

bool PinoParser::canParse(const std::string& content) const {
    if (content.empty()) return false;

    std::istringstream stream(content);
    std::string line;
    int pino_lines = 0;
    int checked = 0;

    // Pino detection: has numeric "level" and "time"
    static std::regex pino_detect(R"("level"\s*:\s*\d+.*"time"\s*:\s*\d+|"time"\s*:\s*\d+.*"level"\s*:\s*\d+)", std::regex::optimize);

    while (std::getline(stream, line) && checked < 10) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty()) continue;

        checked++;

        if (std::regex_search(line, pino_detect)) {
            pino_lines++;
        }
    }

    return pino_lines > 0 && pino_lines >= (checked / 3);
}

std::vector<ValidationEvent> PinoParser::parse(const std::string& content) const {
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
        if (ParsePinoLine(line, event, event_id, line_number)) {
            events.push_back(event);
            event_id++;
        }
    }

    return events;
}

} // namespace duckdb
