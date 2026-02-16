#include "github_cli_parser.hpp"
#include <sstream>
#include <regex>

namespace duckdb {

// Pre-compiled regex patterns for GitHub CLI parsing (compiled once, reused)
namespace {
// canParse patterns
static const std::regex RE_RUN_ENTRY(R"([✓X]\s+\w+\s+(completed|in_progress|cancelled)\s+.+\s+\w+\s+\d+[mhd])");
static const std::regex RE_JOB_STATUS(R"([✓X]\s+\w+\s+(Success|Failure|Cancelled|Skipped))");
static const std::regex RE_STEP_TIMESTAMP(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z)");
// parseRunsList patterns
static const std::regex RE_RUN_PATTERN(R"(([✓X])\s+(\w+)\s+(\w+)\s+(.+?)\s+(\w+)\s+(\d+[mhd]|[a-zA-Z]+\s+\d+))");
// parseRunView patterns
static const std::regex RE_RUN_ID(R"(Run #?(\d+)|Run ID:\s*(\d+))");
static const std::regex RE_STATUS_PATTERN(R"(Status:\s*(\w+))");
static const std::regex RE_CONCLUSION_PATTERN(R"(Conclusion:\s*(\w+))");
static const std::regex RE_JOB_RESULT(R"(([✓X])\s+(.+?)\s+(Success|Failure|Cancelled|Skipped))");
// parseWorkflowLog patterns
static const std::regex RE_TIMESTAMP(R"((\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z)\s*(.+))");
static const std::regex RE_ERROR(R"(::error::(.+))");
static const std::regex RE_WARNING(R"(::warning::(.+))");
static const std::regex RE_NOTICE(R"(::notice::(.+))");
static const std::regex RE_GROUP(R"(##\[group\](.+))");
static const std::regex RE_ENDGROUP(R"(##\[endgroup\])");
static const std::regex RE_STEP(R"(Run (.+)|Setup (.+))");
} // anonymous namespace

bool GitHubCliParser::canParse(const std::string &content) const {
	return isGitHubRunsList(content) || isGitHubRunView(content) || isGitHubWorkflowLog(content);
}

bool GitHubCliParser::isGitHubRunsList(const std::string &content) const {
	// Check for "gh run list" output patterns
	if (content.find("STATUS") != std::string::npos && content.find("CONCLUSION") != std::string::npos &&
	    content.find("WORKFLOW") != std::string::npos && content.find("BRANCH") != std::string::npos) {
		return true;
	}

	// Also check for run entries with specific patterns
	return std::regex_search(content, RE_RUN_ENTRY);
}

bool GitHubCliParser::isGitHubRunView(const std::string &content) const {
	// Check for "gh run view" output patterns
	if ((content.find("Run #") != std::string::npos || content.find("Run ID:") != std::string::npos) &&
	    (content.find("Status:") != std::string::npos || content.find("Conclusion:") != std::string::npos)) {
		return true;
	}

	// Check for job status patterns
	return std::regex_search(content, RE_JOB_STATUS);
}

bool GitHubCliParser::isGitHubWorkflowLog(const std::string &content) const {
	// Check for GitHub Actions workflow log patterns
	if (content.find("##[group]") != std::string::npos || content.find("##[endgroup]") != std::string::npos ||
	    content.find("::error::") != std::string::npos || content.find("::warning::") != std::string::npos ||
	    content.find("::notice::") != std::string::npos) {
		return true;
	}

	// Check for step output patterns
	if (std::regex_search(content, RE_STEP_TIMESTAMP)) {
		return content.find("Run ") != std::string::npos || content.find("Setup ") != std::string::npos;
	}

	// Check for combined annotation patterns (from main detection logic)
	if ((content.find("::error::") != std::string::npos && content.find("::warning::") != std::string::npos) ||
	    (content.find("##[endgroup]") != std::string::npos && content.find("::notice::") != std::string::npos)) {
		return true;
	}

	return false;
}

std::vector<ValidationEvent> GitHubCliParser::parse(const std::string &content) const {
	if (isGitHubRunsList(content)) {
		return parseRunsList(content);
	} else if (isGitHubRunView(content)) {
		return parseRunView(content);
	} else if (isGitHubWorkflowLog(content)) {
		return parseWorkflowLog(content);
	}

	return {};
}

std::vector<ValidationEvent> GitHubCliParser::parseRunsList(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	// Skip header line
	if (std::getline(stream, line) && line.find("STATUS") != std::string::npos) {
		current_line_num++;
		// This is the header, continue to data lines
	}

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;
		if (std::regex_search(line, match, RE_RUN_PATTERN)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "github-cli";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.category = "ci_run";

			std::string icon = match[1].str();
			std::string status = match[2].str();
			std::string conclusion = match[3].str();
			std::string workflow = match[4].str();
			std::string branch = match[5].str();
			std::string time = match[6].str();

			// Set status based on conclusion and icon
			if (icon == "✓" && conclusion == "success") {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
			} else if (icon == "X" && conclusion == "failure") {
				event.status = ValidationEventStatus::ERROR;
				event.severity = "error";
			} else if (conclusion == "cancelled") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "warning";
			} else {
				event.status = ValidationEventStatus::WARNING;
				event.severity = "warning";
			}

			event.message = "Workflow '" + workflow + "' " + conclusion + " on branch '" + branch + "'";
			event.function_name = workflow;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"status\": \"" + status + "\", \"conclusion\": \"" + conclusion +
			                        "\", \"branch\": \"" + branch + "\", \"time\": \"" + time + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
	}

	return events;
}

std::vector<ValidationEvent> GitHubCliParser::parseRunView(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;
	std::string run_id, run_status, run_conclusion;

	// Parse run metadata first
	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;
		if (line.find("Run #") != std::string::npos || line.find("Run ID:") != std::string::npos) {
			if (std::regex_search(line, match, RE_RUN_ID)) {
				run_id = match[1].str().empty() ? match[2].str() : match[1].str();
			}
		} else if (line.find("Status:") != std::string::npos) {
			if (std::regex_search(line, match, RE_STATUS_PATTERN)) {
				run_status = match[1].str();
			}
		} else if (line.find("Conclusion:") != std::string::npos) {
			if (std::regex_search(line, match, RE_CONCLUSION_PATTERN)) {
				run_conclusion = match[1].str();
			}
		}

		// Parse job status lines: [✓X] job_name Success/Failure
		std::smatch job_match;
		if (std::regex_search(line, job_match, RE_JOB_RESULT)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "github-cli";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.category = "ci_job";

			std::string icon = job_match[1].str();
			std::string job_name = job_match[2].str();
			std::string job_status = job_match[3].str();

			// Set status based on job result
			if (job_status == "Success") {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
			} else if (job_status == "Failure") {
				event.status = ValidationEventStatus::ERROR;
				event.severity = "error";
			} else if (job_status == "Cancelled") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "warning";
			} else if (job_status == "Skipped") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "info";
			} else {
				event.status = ValidationEventStatus::WARNING;
				event.severity = "warning";
			}

			event.message = "Job '" + job_name + "' " + job_status;
			event.function_name = job_name;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"run_id\": \"" + run_id + "\", \"job_status\": \"" + job_status + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
	}

	return events;
}

std::vector<ValidationEvent> GitHubCliParser::parseWorkflowLog(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;
	std::string current_step;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Parse group markers (step names)
		if (std::regex_search(line, match, RE_GROUP)) {
			current_step = match[1].str();
			continue;
		}

		if (std::regex_search(line, match, RE_ENDGROUP)) {
			current_step.clear();
			continue;
		}

		// Parse step start markers
		if (std::regex_search(line, match, RE_STEP)) {
			std::string step_name = match[1].str().empty() ? match[2].str() : match[1].str();
			current_step = step_name;
			continue;
		}

		// Parse error messages
		if (std::regex_search(line, match, RE_ERROR)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "github-actions";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.category = "workflow_error";
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.message = match[1].str();
			event.function_name = current_step;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"step\": \"" + current_step + "\", \"type\": \"error\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}

		// Parse warning messages
		else if (std::regex_search(line, match, RE_WARNING)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "github-actions";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.category = "workflow_warning";
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.message = match[1].str();
			event.function_name = current_step;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"step\": \"" + current_step + "\", \"type\": \"warning\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}

		// Parse notice messages
		else if (std::regex_search(line, match, RE_NOTICE)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "github-actions";
			event.event_type = ValidationEventType::SUMMARY;
			event.category = "workflow_notice";
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.message = match[1].str();
			event.function_name = current_step;
			event.ref_line = -1;
			event.ref_column = -1;
			event.execution_time = 0.0;
			event.log_content = line;
			event.structured_data = "{\"step\": \"" + current_step + "\", \"type\": \"notice\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
