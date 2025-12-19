#include "tfsec_json_parser.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool TfsecJSONParser::canParse(const std::string& content) const {
    // Quick checks for tfsec JSON patterns
    // tfsec has results array with rule_id, rule_description, severity, location
    if (content.find("\"results\"") != std::string::npos &&
        content.find("\"rule_id\"") != std::string::npos &&
        content.find("\"rule_description\"") != std::string::npos &&
        content.find("\"location\"") != std::string::npos) {
        return isValidTfsecJSON(content);
    }
    return false;
}

bool TfsecJSONParser::isValidTfsecJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;

    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;

    if (yyjson_is_obj(root)) {
        yyjson_val *results = yyjson_obj_get(root, "results");
        if (results && yyjson_is_arr(results)) {
            // Check if first element has tfsec structure
            size_t idx, max;
            yyjson_val *issue;
            yyjson_arr_foreach(results, idx, max, issue) {
                if (yyjson_is_obj(issue)) {
                    yyjson_val *rule_id = yyjson_obj_get(issue, "rule_id");
                    yyjson_val *severity = yyjson_obj_get(issue, "severity");
                    yyjson_val *location = yyjson_obj_get(issue, "location");

                    if (rule_id && yyjson_is_str(rule_id) &&
                        severity && yyjson_is_str(severity) &&
                        location && yyjson_is_obj(location)) {
                        is_valid = true;
                        break;
                    }
                }
            }
        }
    }

    yyjson_doc_free(doc);
    return is_valid;
}

std::vector<ValidationEvent> TfsecJSONParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;

    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) {
        return events;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return events;
    }

    // Get results array
    yyjson_val *results = yyjson_obj_get(root, "results");
    if (!results || !yyjson_is_arr(results)) {
        yyjson_doc_free(doc);
        return events;
    }

    int64_t event_id = 1;
    size_t idx, max;
    yyjson_val *issue;

    yyjson_arr_foreach(results, idx, max, issue) {
        if (!yyjson_is_obj(issue)) continue;

        ValidationEvent event;
        event.event_id = event_id++;
        event.tool_name = "tfsec";
        event.event_type = ValidationEventType::SECURITY_FINDING;
        event.category = "infrastructure_security";
        event.execution_time = 0.0;

        // Get rule ID
        yyjson_val *rule_id = yyjson_obj_get(issue, "rule_id");
        if (rule_id && yyjson_is_str(rule_id)) {
            event.error_code = yyjson_get_str(rule_id);
        }

        // Get rule description as message
        yyjson_val *rule_desc = yyjson_obj_get(issue, "rule_description");
        if (rule_desc && yyjson_is_str(rule_desc)) {
            event.message = yyjson_get_str(rule_desc);
        }

        // Get description (more detailed)
        yyjson_val *description = yyjson_obj_get(issue, "description");
        if (description && yyjson_is_str(description)) {
            if (event.message.empty()) {
                event.message = yyjson_get_str(description);
            } else {
                event.message += ": " + std::string(yyjson_get_str(description));
            }
        }

        // Get resource as function name
        yyjson_val *resource = yyjson_obj_get(issue, "resource");
        if (resource && yyjson_is_str(resource)) {
            event.function_name = yyjson_get_str(resource);
        }

        // Get severity and map to status
        yyjson_val *severity = yyjson_obj_get(issue, "severity");
        if (severity && yyjson_is_str(severity)) {
            std::string sev_str = yyjson_get_str(severity);
            event.severity = sev_str;

            if (sev_str == "CRITICAL" || sev_str == "HIGH") {
                event.status = ValidationEventStatus::ERROR;
            } else if (sev_str == "MEDIUM") {
                event.status = ValidationEventStatus::WARNING;
            } else {
                event.status = ValidationEventStatus::INFO;
            }
        } else {
            event.severity = "MEDIUM";
            event.status = ValidationEventStatus::WARNING;
        }

        // Get location information
        yyjson_val *location = yyjson_obj_get(issue, "location");
        if (location && yyjson_is_obj(location)) {
            yyjson_val *filename = yyjson_obj_get(location, "filename");
            yyjson_val *start_line = yyjson_obj_get(location, "start_line");
            // Note: end_line is available but not currently used

            if (filename && yyjson_is_str(filename)) {
                event.ref_file = yyjson_get_str(filename);
            }

            if (start_line && yyjson_is_num(start_line)) {
                event.ref_line = yyjson_get_int(start_line);
            } else {
                event.ref_line = -1;
            }

            event.ref_column = -1;
        } else {
            event.ref_line = -1;
            event.ref_column = -1;
        }

        // Get resolution as suggestion
        yyjson_val *resolution = yyjson_obj_get(issue, "resolution");
        if (resolution && yyjson_is_str(resolution)) {
            event.suggestion = yyjson_get_str(resolution);
        }

        // Get impact for additional context
        yyjson_val *impact = yyjson_obj_get(issue, "impact");
        std::string impact_str;
        if (impact && yyjson_is_str(impact)) {
            impact_str = ", \"impact\": \"" + std::string(yyjson_get_str(impact)) + "\"";
        }

        // Get provider and service info
        yyjson_val *provider = yyjson_obj_get(issue, "rule_provider");
        yyjson_val *service = yyjson_obj_get(issue, "rule_service");
        std::string provider_str, service_str;
        if (provider && yyjson_is_str(provider)) {
            provider_str = yyjson_get_str(provider);
        }
        if (service && yyjson_is_str(service)) {
            service_str = yyjson_get_str(service);
        }

        event.log_content = content;
        event.structured_data = "{\"tool\": \"tfsec\", \"rule_id\": \"" + event.error_code +
                               "\", \"severity\": \"" + event.severity +
                               "\", \"resource\": \"" + event.function_name + "\"" +
                               (provider_str.empty() ? "" : ", \"provider\": \"" + provider_str + "\"") +
                               (service_str.empty() ? "" : ", \"service\": \"" + service_str + "\"") +
                               impact_str + "}";

        events.push_back(event);
    }

    yyjson_doc_free(doc);
    return events;
}

} // namespace duckdb
