#include "terraform_text_parser.hpp"
#include "core/legacy_parser_registry.hpp"
#include "duckdb/common/exception.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

bool TerraformTextParser::canParse(const std::string& content) const {
    return isValidTerraformText(content);
}

bool TerraformTextParser::isValidTerraformText(const std::string& content) const {
    // Look for Terraform specific patterns
    if (content.find("Terraform") != std::string::npos ||
        content.find("terraform") != std::string::npos ||
        content.find("Plan:") != std::string::npos ||
        content.find("Apply complete!") != std::string::npos ||
        content.find("will be created") != std::string::npos ||
        content.find("provider registry.terraform.io") != std::string::npos) {
        return true;
    }
    return false;
}

std::vector<ValidationEvent> TerraformTextParser::parse(const std::string& content) const {
    std::vector<ValidationEvent> events;
    std::istringstream stream(content);
    std::string line;
    int64_t event_id = 1;
    int32_t current_line_num = 0;
    std::smatch match;

    // Terraform regex patterns
    std::regex terraform_version(R"(Terraform v(\d+\.\d+\.\d+))");
    std::regex provider_info(R"(\+ provider registry\.terraform\.io/hashicorp/(\w+) v([\d\.]+))");
    std::regex resource_create(R"(# (\S+) will be created)");
    std::regex resource_update(R"(# (\S+) will be updated in-place)");
    std::regex resource_destroy(R"(# (\S+) will be destroyed)");
    std::regex plan_summary(R"(Plan: (\d+) to add, (\d+) to change, (\d+) to destroy)");
    std::regex resource_creating(R"((\S+): Creating\.\.\.)");
    std::regex resource_creation_complete(R"((\S+): Creation complete after (\d+)s \[id=(.+)\])");
    std::regex resource_modifying(R"((\S+): Modifying\.\.\. \[id=(.+)\])");
    std::regex resource_modification_complete(R"((\S+): Modifications complete after (\d+)s \[id=(.+)\])");
    std::regex resource_destroying(R"((\S+): Destroying\.\.\. \[id=(.+)\])");
    std::regex resource_destruction_complete(R"((\S+): Destruction complete after (\d+)s)");
    std::regex apply_complete(R"(Apply complete! Resources: (\d+) added, (\d+) changed, (\d+) destroyed)");
    std::regex terraform_error(R"(Error: (.+))");
    std::regex terraform_warning(R"(Warning: (.+))");
    std::regex terraform_output(R"((\w+) = (.+))");
    std::regex version_warning(R"(Your version of Terraform is out of date!)");
    
    while (std::getline(stream, line)) {
        current_line_num++;
        // Parse Terraform version
        if (std::regex_search(line, match, terraform_version)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "version";
            event.message = "Terraform version: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse provider information
        if (std::regex_search(line, match, provider_info)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "provider";
            event.message = "Provider " + match[1].str() + " version " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource creation plan
        if (std::regex_search(line, match, resource_create)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "plan_create";
            event.message = "Resource will be created: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource update plan
        if (std::regex_search(line, match, resource_update)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "plan_update";
            event.message = "Resource will be updated in-place: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource destroy plan
        if (std::regex_search(line, match, resource_destroy)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "plan_destroy";
            event.message = "Resource will be destroyed: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse plan summary
        if (std::regex_search(line, match, plan_summary)) {
            int to_add = std::stoi(match[1].str());
            int to_change = std::stoi(match[2].str());
            int to_destroy = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = to_destroy > 0 ? ValidationEventStatus::WARNING : ValidationEventStatus::INFO;
            event.severity = to_destroy > 0 ? "warning" : "info";
            event.category = "plan_summary";
            event.message = "Plan: " + std::to_string(to_add) + " to add, " + 
                           std::to_string(to_change) + " to change, " + 
                           std::to_string(to_destroy) + " to destroy";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource creation in progress
        if (std::regex_search(line, match, resource_creating)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "resource_creating";
            event.message = "Creating resource: " + match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource creation complete
        if (std::regex_search(line, match, resource_creation_complete)) {
            std::string resource_name = match[1].str();
            int duration = std::stoi(match[2].str());
            std::string resource_id = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "resource_created";
            event.message = "Creation complete: " + resource_name + " [id=" + resource_id + "]";
            event.execution_time = duration;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource modification in progress
        if (std::regex_search(line, match, resource_modifying)) {
            std::string resource_name = match[1].str();
            std::string resource_id = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "resource_modifying";
            event.message = "Modifying resource: " + resource_name + " [id=" + resource_id + "]";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource modification complete
        if (std::regex_search(line, match, resource_modification_complete)) {
            std::string resource_name = match[1].str();
            int duration = std::stoi(match[2].str());
            std::string resource_id = match[3].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "resource_modified";
            event.message = "Modifications complete: " + resource_name + " [id=" + resource_id + "]";
            event.execution_time = duration;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource destruction in progress
        if (std::regex_search(line, match, resource_destroying)) {
            std::string resource_name = match[1].str();
            std::string resource_id = match[2].str();
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "resource_destroying";
            event.message = "Destroying resource: " + resource_name + " [id=" + resource_id + "]";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse resource destruction complete
        if (std::regex_search(line, match, resource_destruction_complete)) {
            std::string resource_name = match[1].str();
            int duration = std::stoi(match[2].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "resource_destroyed";
            event.message = "Destruction complete: " + resource_name;
            event.execution_time = duration;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse apply complete summary
        if (std::regex_search(line, match, apply_complete)) {
            int added = std::stoi(match[1].str());
            int changed = std::stoi(match[2].str());
            int destroyed = std::stoi(match[3].str());
            
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::SUMMARY;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::PASS;
            event.severity = "info";
            event.category = "apply_complete";
            event.message = "Apply complete! Resources: " + std::to_string(added) + " added, " + 
                           std::to_string(changed) + " changed, " + std::to_string(destroyed) + " destroyed";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse Terraform errors
        if (std::regex_search(line, match, terraform_error)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::ERROR;
            event.severity = "error";
            event.category = "terraform_error";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse Terraform warnings
        if (std::regex_search(line, match, terraform_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "terraform_warning";
            event.message = match[1].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse version warnings
        if (std::regex_search(line, match, version_warning)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::LINT_ISSUE;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::WARNING;
            event.severity = "warning";
            event.category = "version_warning";
            event.message = "Your version of Terraform is out of date!";
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
        
        // Parse Terraform outputs
        if (std::regex_search(line, match, terraform_output)) {
            ValidationEvent event;
            event.event_id = event_id++;
            event.tool_name = "terraform";
            event.event_type = ValidationEventType::DEBUG_EVENT;
            event.file_path = "";
            event.line_number = -1;
            event.column_number = -1;
            event.status = ValidationEventStatus::INFO;
            event.severity = "info";
            event.category = "terraform_output";
            event.message = "Output: " + match[1].str() + " = " + match[2].str();
            event.execution_time = 0.0;
            event.raw_output = content;
            event.structured_data = "terraform_text";
            event.log_line_start = current_line_num;
            event.log_line_end = current_line_num;

            events.push_back(event);
            continue;
        }
    }
    
    return events;
}

// Auto-register this parser
REGISTER_PARSER(TerraformTextParser);

} // namespace duckdb