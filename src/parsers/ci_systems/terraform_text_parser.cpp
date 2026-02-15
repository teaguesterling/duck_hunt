#include "terraform_text_parser.hpp"
#include "duckdb/common/exception.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Pre-compiled regex patterns for Terraform text parsing (compiled once, reused)
namespace {
static const std::regex RE_TERRAFORM_VERSION(R"(Terraform v(\d+\.\d+\.\d+))");
static const std::regex RE_PROVIDER_INFO(R"(\+ provider registry\.terraform\.io/hashicorp/(\w+) v([\d\.]+))");
static const std::regex RE_RESOURCE_CREATE(R"(# (\S+) will be created)");
static const std::regex RE_RESOURCE_UPDATE(R"(# (\S+) will be updated in-place)");
static const std::regex RE_RESOURCE_DESTROY(R"(# (\S+) will be destroyed)");
static const std::regex RE_PLAN_SUMMARY(R"(Plan: (\d+) to add, (\d+) to change, (\d+) to destroy)");
static const std::regex RE_RESOURCE_CREATING(R"((\S+): Creating\.\.\.)");
static const std::regex RE_RESOURCE_CREATION_COMPLETE(R"((\S+): Creation complete after (\d+)s \[id=(.+)\])");
static const std::regex RE_RESOURCE_MODIFYING(R"((\S+): Modifying\.\.\. \[id=(.+)\])");
static const std::regex RE_RESOURCE_MODIFICATION_COMPLETE(R"((\S+): Modifications complete after (\d+)s \[id=(.+)\])");
static const std::regex RE_RESOURCE_DESTROYING(R"((\S+): Destroying\.\.\. \[id=(.+)\])");
static const std::regex RE_RESOURCE_DESTRUCTION_COMPLETE(R"((\S+): Destruction complete after (\d+)s)");
static const std::regex RE_APPLY_COMPLETE(R"(Apply complete! Resources: (\d+) added, (\d+) changed, (\d+) destroyed)");
static const std::regex RE_TERRAFORM_ERROR(R"(Error: (.+))");
static const std::regex RE_TERRAFORM_WARNING(R"(Warning: (.+))");
static const std::regex RE_TERRAFORM_OUTPUT(R"((\w+) = (.+))");
static const std::regex RE_VERSION_WARNING(R"(Your version of Terraform is out of date!)");
} // anonymous namespace

bool TerraformTextParser::canParse(const std::string &content) const {
	return isValidTerraformText(content);
}

bool TerraformTextParser::isValidTerraformText(const std::string &content) const {
	// Look for Terraform specific patterns
	if (content.find("Terraform") != std::string::npos || content.find("terraform") != std::string::npos ||
	    content.find("Plan:") != std::string::npos || content.find("Apply complete!") != std::string::npos ||
	    content.find("will be created") != std::string::npos ||
	    content.find("provider registry.terraform.io") != std::string::npos) {
		return true;
	}
	return false;
}

std::vector<ValidationEvent> TerraformTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;
	std::smatch match;

	while (std::getline(stream, line)) {
		current_line_num++;
		// Parse Terraform version
		if (std::regex_search(line, match, RE_TERRAFORM_VERSION)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "version";
			event.message = "Terraform version: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse provider information
		if (std::regex_search(line, match, RE_PROVIDER_INFO)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "provider";
			event.message = "Provider " + match[1].str() + " version " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource creation plan
		if (std::regex_search(line, match, RE_RESOURCE_CREATE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "plan_create";
			event.message = "Resource will be created: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource update plan
		if (std::regex_search(line, match, RE_RESOURCE_UPDATE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "plan_update";
			event.message = "Resource will be updated in-place: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource destroy plan
		if (std::regex_search(line, match, RE_RESOURCE_DESTROY)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "plan_destroy";
			event.message = "Resource will be destroyed: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse plan summary
		if (std::regex_search(line, match, RE_PLAN_SUMMARY)) {
			int to_add = std::stoi(match[1].str());
			int to_change = std::stoi(match[2].str());
			int to_destroy = std::stoi(match[3].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = to_destroy > 0 ? ValidationEventStatus::WARNING : ValidationEventStatus::INFO;
			event.severity = to_destroy > 0 ? "warning" : "info";
			event.category = "plan_summary";
			event.message = "Plan: " + std::to_string(to_add) + " to add, " + std::to_string(to_change) +
			                " to change, " + std::to_string(to_destroy) + " to destroy";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource creation in progress
		if (std::regex_search(line, match, RE_RESOURCE_CREATING)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "resource_creating";
			event.message = "Creating resource: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource creation complete
		if (std::regex_search(line, match, RE_RESOURCE_CREATION_COMPLETE)) {
			std::string resource_name = match[1].str();
			int duration = std::stoi(match[2].str());
			std::string resource_id = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "resource_created";
			event.message = "Creation complete: " + resource_name + " [id=" + resource_id + "]";
			event.execution_time = duration;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource modification in progress
		if (std::regex_search(line, match, RE_RESOURCE_MODIFYING)) {
			std::string resource_name = match[1].str();
			std::string resource_id = match[2].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "resource_modifying";
			event.message = "Modifying resource: " + resource_name + " [id=" + resource_id + "]";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource modification complete
		if (std::regex_search(line, match, RE_RESOURCE_MODIFICATION_COMPLETE)) {
			std::string resource_name = match[1].str();
			int duration = std::stoi(match[2].str());
			std::string resource_id = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "resource_modified";
			event.message = "Modifications complete: " + resource_name + " [id=" + resource_id + "]";
			event.execution_time = duration;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource destruction in progress
		if (std::regex_search(line, match, RE_RESOURCE_DESTROYING)) {
			std::string resource_name = match[1].str();
			std::string resource_id = match[2].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "resource_destroying";
			event.message = "Destroying resource: " + resource_name + " [id=" + resource_id + "]";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse resource destruction complete
		if (std::regex_search(line, match, RE_RESOURCE_DESTRUCTION_COMPLETE)) {
			std::string resource_name = match[1].str();
			int duration = std::stoi(match[2].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "resource_destroyed";
			event.message = "Destruction complete: " + resource_name;
			event.execution_time = duration;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse apply complete summary
		if (std::regex_search(line, match, RE_APPLY_COMPLETE)) {
			int added = std::stoi(match[1].str());
			int changed = std::stoi(match[2].str());
			int destroyed = std::stoi(match[3].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "apply_complete";
			event.message = "Apply complete! Resources: " + std::to_string(added) + " added, " +
			                std::to_string(changed) + " changed, " + std::to_string(destroyed) + " destroyed";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse Terraform errors
		if (std::regex_search(line, match, RE_TERRAFORM_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "terraform_error";
			event.message = match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse Terraform warnings
		if (std::regex_search(line, match, RE_TERRAFORM_WARNING)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "terraform_warning";
			event.message = match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse version warnings
		if (std::regex_search(line, match, RE_VERSION_WARNING)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "version_warning";
			event.message = "Your version of Terraform is out of date!";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}

		// Parse Terraform outputs
		if (std::regex_search(line, match, RE_TERRAFORM_OUTPUT)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "terraform";
			event.event_type = ValidationEventType::DEBUG_EVENT;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "terraform_output";
			event.message = "Output: " + match[1].str() + " = " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "terraform_text";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
			continue;
		}
	}

	return events;
}

} // namespace duckdb
