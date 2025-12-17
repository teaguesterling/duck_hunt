#include "trivy_json_parser.hpp"
#include "yyjson.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool TrivyJSONParser::canParse(const std::string& content) const {
    // Quick checks for Trivy JSON patterns
    // Trivy has Results array with either Vulnerabilities or Misconfigurations
    if (content.find("\"Results\"") != std::string::npos &&
        (content.find("\"Vulnerabilities\"") != std::string::npos ||
         content.find("\"Misconfigurations\"") != std::string::npos) &&
        (content.find("\"VulnerabilityID\"") != std::string::npos ||
         content.find("\"SchemaVersion\"") != std::string::npos)) {
        return isValidTrivyJSON(content);
    }
    return false;
}

bool TrivyJSONParser::isValidTrivyJSON(const std::string& content) const {
    yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
    if (!doc) return false;

    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_valid = false;

    if (yyjson_is_obj(root)) {
        yyjson_val *results = yyjson_obj_get(root, "Results");
        if (results && yyjson_is_arr(results)) {
            // Check if any result has Vulnerabilities or Misconfigurations
            size_t idx, max;
            yyjson_val *result;
            yyjson_arr_foreach(results, idx, max, result) {
                if (yyjson_is_obj(result)) {
                    yyjson_val *vulns = yyjson_obj_get(result, "Vulnerabilities");
                    yyjson_val *misconfigs = yyjson_obj_get(result, "Misconfigurations");

                    if ((vulns && yyjson_is_arr(vulns)) ||
                        (misconfigs && yyjson_is_arr(misconfigs))) {
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

std::vector<ValidationEvent> TrivyJSONParser::parse(const std::string& content) const {
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

    // Get artifact name for context
    std::string artifact_name;
    yyjson_val *artifact = yyjson_obj_get(root, "ArtifactName");
    if (artifact && yyjson_is_str(artifact)) {
        artifact_name = yyjson_get_str(artifact);
    }

    // Get results array
    yyjson_val *results = yyjson_obj_get(root, "Results");
    if (!results || !yyjson_is_arr(results)) {
        yyjson_doc_free(doc);
        return events;
    }

    int64_t event_id = 1;
    size_t result_idx, result_max;
    yyjson_val *result;

    yyjson_arr_foreach(results, result_idx, result_max, result) {
        if (!yyjson_is_obj(result)) continue;

        // Get target for file context
        std::string target;
        yyjson_val *target_val = yyjson_obj_get(result, "Target");
        if (target_val && yyjson_is_str(target_val)) {
            target = yyjson_get_str(target_val);
        }

        // Get result type/class
        std::string result_class;
        yyjson_val *class_val = yyjson_obj_get(result, "Class");
        if (class_val && yyjson_is_str(class_val)) {
            result_class = yyjson_get_str(class_val);
        }

        // Parse Vulnerabilities
        yyjson_val *vulns = yyjson_obj_get(result, "Vulnerabilities");
        if (vulns && yyjson_is_arr(vulns)) {
            size_t vuln_idx, vuln_max;
            yyjson_val *vuln;

            yyjson_arr_foreach(vulns, vuln_idx, vuln_max, vuln) {
                if (!yyjson_is_obj(vuln)) continue;

                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "trivy";
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "vulnerability";
                event.execution_time = 0.0;
                event.file_path = target;
                event.line_number = -1;
                event.column_number = -1;

                // Get vulnerability ID
                yyjson_val *vuln_id = yyjson_obj_get(vuln, "VulnerabilityID");
                if (vuln_id && yyjson_is_str(vuln_id)) {
                    event.error_code = yyjson_get_str(vuln_id);
                }

                // Get package name
                yyjson_val *pkg_name = yyjson_obj_get(vuln, "PkgName");
                if (pkg_name && yyjson_is_str(pkg_name)) {
                    event.function_name = yyjson_get_str(pkg_name);
                }

                // Get severity and map to status
                yyjson_val *severity = yyjson_obj_get(vuln, "Severity");
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
                    event.severity = "UNKNOWN";
                    event.status = ValidationEventStatus::WARNING;
                }

                // Build message from title or description
                yyjson_val *title = yyjson_obj_get(vuln, "Title");
                yyjson_val *description = yyjson_obj_get(vuln, "Description");
                if (title && yyjson_is_str(title)) {
                    event.message = yyjson_get_str(title);
                } else if (description && yyjson_is_str(description)) {
                    event.message = yyjson_get_str(description);
                }

                // Build suggestion from fixed version
                yyjson_val *fixed_version = yyjson_obj_get(vuln, "FixedVersion");
                yyjson_val *installed_version = yyjson_obj_get(vuln, "InstalledVersion");
                if (fixed_version && yyjson_is_str(fixed_version)) {
                    std::string suggestion = "Upgrade to version " + std::string(yyjson_get_str(fixed_version));
                    if (installed_version && yyjson_is_str(installed_version)) {
                        suggestion += " (current: " + std::string(yyjson_get_str(installed_version)) + ")";
                    }
                    event.suggestion = suggestion;
                }

                // Get CVSS score if available
                yyjson_val *cvss = yyjson_obj_get(vuln, "CVSS");
                std::string cvss_info;
                if (cvss && yyjson_is_obj(cvss)) {
                    yyjson_val *nvd = yyjson_obj_get(cvss, "nvd");
                    if (nvd && yyjson_is_obj(nvd)) {
                        yyjson_val *v3_score = yyjson_obj_get(nvd, "V3Score");
                        if (v3_score && yyjson_is_num(v3_score)) {
                            cvss_info = ", \"cvss_score\": " + std::to_string(yyjson_get_real(v3_score));
                        }
                    }
                }

                event.raw_output = content;
                event.structured_data = "{\"tool\": \"trivy\", \"vuln_id\": \"" + event.error_code +
                                       "\", \"severity\": \"" + event.severity +
                                       "\", \"package\": \"" + event.function_name + "\"" + cvss_info + "}";

                events.push_back(event);
            }
        }

        // Parse Misconfigurations
        yyjson_val *misconfigs = yyjson_obj_get(result, "Misconfigurations");
        if (misconfigs && yyjson_is_arr(misconfigs)) {
            size_t mc_idx, mc_max;
            yyjson_val *misconfig;

            yyjson_arr_foreach(misconfigs, mc_idx, mc_max, misconfig) {
                if (!yyjson_is_obj(misconfig)) continue;

                ValidationEvent event;
                event.event_id = event_id++;
                event.tool_name = "trivy";
                event.event_type = ValidationEventType::SECURITY_FINDING;
                event.category = "misconfiguration";
                event.execution_time = 0.0;
                event.file_path = target;
                event.line_number = -1;
                event.column_number = -1;

                // Get misconfiguration ID
                yyjson_val *mc_id = yyjson_obj_get(misconfig, "ID");
                if (mc_id && yyjson_is_str(mc_id)) {
                    event.error_code = yyjson_get_str(mc_id);
                }

                // Get type as function context
                yyjson_val *mc_type = yyjson_obj_get(misconfig, "Type");
                if (mc_type && yyjson_is_str(mc_type)) {
                    event.function_name = yyjson_get_str(mc_type);
                }

                // Get severity
                yyjson_val *severity = yyjson_obj_get(misconfig, "Severity");
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
                    event.severity = "UNKNOWN";
                    event.status = ValidationEventStatus::WARNING;
                }

                // Get title and description
                yyjson_val *title = yyjson_obj_get(misconfig, "Title");
                yyjson_val *message_val = yyjson_obj_get(misconfig, "Message");
                yyjson_val *description = yyjson_obj_get(misconfig, "Description");

                if (title && yyjson_is_str(title)) {
                    event.message = yyjson_get_str(title);
                    if (message_val && yyjson_is_str(message_val)) {
                        event.message += ": " + std::string(yyjson_get_str(message_val));
                    }
                } else if (description && yyjson_is_str(description)) {
                    event.message = yyjson_get_str(description);
                }

                // Get resolution as suggestion
                yyjson_val *resolution = yyjson_obj_get(misconfig, "Resolution");
                if (resolution && yyjson_is_str(resolution)) {
                    event.suggestion = yyjson_get_str(resolution);
                }

                event.raw_output = content;
                event.structured_data = "{\"tool\": \"trivy\", \"config_id\": \"" + event.error_code +
                                       "\", \"severity\": \"" + event.severity + "\"}";

                events.push_back(event);
            }
        }
    }

    yyjson_doc_free(doc);
    return events;
}

} // namespace duckdb
