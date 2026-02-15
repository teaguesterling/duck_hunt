#include "github_actions_text_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for GitHub Actions text parsing (compiled once, reused)
namespace {
static const std::regex RE_ERROR_CMD(R"(::error(?:\s+file=([^,]+)(?:,line=(\d+))?)?::(.+))");
static const std::regex RE_WARNING_CMD(R"(::warning(?:\s+file=([^,]+)(?:,line=(\d+))?)?::(.+))");
static const std::regex RE_NOTICE_CMD(R"(::notice(?:\s+file=([^,]+)(?:,line=(\d+))?)?::(.+))");
static const std::regex RE_GROUP_CMD(R"(::group::(.+))");
static const std::regex RE_ENDGROUP_CMD(R"(::endgroup::)");
static const std::regex RE_BRACKET_ERROR(R"(##\[error\](.+))");
static const std::regex RE_BRACKET_WARNING(R"(##\[warning\](.+))");
static const std::regex RE_BRACKET_GROUP(R"(##\[group\](.+))");
static const std::regex RE_RUN_STEP(R"(^Run\s+(.+))");
static const std::regex RE_EXIT_CODE(R"(Process completed with exit code (\d+))");
static const std::regex RE_SHELL_ERROR(R"(##\[error\]Process completed with exit code (\d+))");
} // anonymous namespace

bool GitHubActionsTextParser::canParse(const std::string &content) const {
	// GitHub Actions specific markers
	bool has_workflow_command =
	    content.find("::error::") != std::string::npos || content.find("::warning::") != std::string::npos ||
	    content.find("::notice::") != std::string::npos || content.find("::group::") != std::string::npos ||
	    content.find("::endgroup::") != std::string::npos;

	if (has_workflow_command) {
		return true;
	}

	// GitHub Actions step format with timestamps
	bool has_step_markers =
	    content.find("##[group]") != std::string::npos || content.find("##[endgroup]") != std::string::npos ||
	    content.find("##[error]") != std::string::npos || content.find("##[warning]") != std::string::npos;

	if (has_step_markers) {
		return true;
	}

	// Azure DevOps style (GitHub Actions runner uses similar format)
	bool has_azure_style =
	    content.find("Task         :") != std::string::npos && content.find("Description  :") != std::string::npos;

	if (has_azure_style &&
	    content.find("==============================================================================") !=
	        std::string::npos) {
		return true;
	}

	// Run steps pattern: "Run <command>"
	bool has_run_step = content.find("\nRun ") != std::string::npos || content.find("Run ") == 0;
	bool has_with_step = content.find("\n  with:") != std::string::npos;

	if (has_run_step && has_with_step) {
		return true;
	}

	return false;
}

std::vector<ValidationEvent> GitHubActionsTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	std::string current_group;
	std::vector<std::string> error_context;
	bool in_error_block = false;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		// ::error:: workflow command
		if (std::regex_search(line, match, RE_ERROR_CMD)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = match[3].str();
			if (match[1].matched) {
				event.ref_file = match[1].str();
			}
			if (match[2].matched) {
				event.ref_line = std::stoi(match[2].str());
			}
			event.tool_name = "github_actions";
			event.category = "github_actions_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_group.empty()) {
				event.scope = current_group;
			}
			events.push_back(event);
		}
		// ::warning:: workflow command
		else if (std::regex_search(line, match, RE_WARNING_CMD)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
			event.message = match[3].str();
			if (match[1].matched) {
				event.ref_file = match[1].str();
			}
			if (match[2].matched) {
				event.ref_line = std::stoi(match[2].str());
			}
			event.tool_name = "github_actions";
			event.category = "github_actions_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_group.empty()) {
				event.scope = current_group;
			}
			events.push_back(event);
		}
		// ::notice:: workflow command
		else if (std::regex_search(line, match, RE_NOTICE_CMD)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = match[3].str();
			if (match[1].matched) {
				event.ref_file = match[1].str();
			}
			if (match[2].matched) {
				event.ref_line = std::stoi(match[2].str());
			}
			event.tool_name = "github_actions";
			event.category = "github_actions_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// ::group:: command
		else if (std::regex_search(line, match, RE_GROUP_CMD)) {
			current_group = match[1].str();
		}
		// ::endgroup:: command
		else if (std::regex_search(line, match, RE_ENDGROUP_CMD)) {
			current_group.clear();
		}
		// ##[error] format
		else if (std::regex_search(line, match, RE_BRACKET_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = match[1].str();
			event.tool_name = "github_actions";
			event.category = "github_actions_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_group.empty()) {
				event.scope = current_group;
			}
			events.push_back(event);
		}
		// ##[warning] format
		else if (std::regex_search(line, match, RE_BRACKET_WARNING)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
			event.message = match[1].str();
			event.tool_name = "github_actions";
			event.category = "github_actions_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_group.empty()) {
				event.scope = current_group;
			}
			events.push_back(event);
		}
		// ##[group] format
		else if (std::regex_search(line, match, RE_BRACKET_GROUP)) {
			current_group = match[1].str();
		}
		// Exit code error
		else if (std::regex_search(line, match, RE_EXIT_CODE)) {
			int code = std::stoi(match[1].str());
			if (code != 0) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::BUILD_ERROR;
				event.severity = "error";
				event.status = ValidationEventStatus::FAIL;
				event.message = "Process exited with code " + match[1].str();
				event.tool_name = "github_actions";
				event.category = "github_actions_text";
				event.log_content = line;
				event.log_line_start = line_num;
				event.log_line_end = line_num;
				if (!current_group.empty()) {
					event.scope = current_group;
				}
				events.push_back(event);
			}
		}
	}

	return events;
}

} // namespace duckdb
