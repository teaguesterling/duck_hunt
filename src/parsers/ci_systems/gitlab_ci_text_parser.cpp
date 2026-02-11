#include "gitlab_ci_text_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

bool GitLabCITextParser::canParse(const std::string &content) const {
	// GitLab CI specific markers
	bool has_gitlab_runner = content.find("Running with gitlab-runner") != std::string::npos ||
	                         content.find("Preparing the \"docker\" executor") != std::string::npos ||
	                         content.find("Preparing the \"shell\" executor") != std::string::npos;

	if (has_gitlab_runner) {
		return true;
	}

	// GitLab CI section markers
	bool has_section_markers =
	    content.find("section_start:") != std::string::npos || content.find("section_end:") != std::string::npos;

	if (has_section_markers) {
		return true;
	}

	// GitLab CI job patterns
	bool has_job_patterns =
	    (content.find("Job succeeded") != std::string::npos || content.find("Job failed") != std::string::npos) &&
	    (content.find("$ ") != std::string::npos || content.find("Fetching changes") != std::string::npos);

	if (has_job_patterns) {
		return true;
	}

	// Git fetch pattern specific to GitLab
	bool has_gitlab_fetch = content.find("Fetching changes with git depth") != std::string::npos ||
	                        (content.find("Created fresh repository") != std::string::npos &&
	                         content.find("Checking out") != std::string::npos);

	if (has_gitlab_fetch) {
		return true;
	}

	// GitLab artifact patterns
	bool has_artifacts = (content.find("Uploading artifacts") != std::string::npos ||
	                      content.find("Downloading artifacts") != std::string::npos) &&
	                     content.find("gitlab") != std::string::npos;

	return has_artifacts;
}

std::vector<ValidationEvent> GitLabCITextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t line_num = 0;

	// Patterns for GitLab CI output
	std::regex section_start(R"(section_start:\d+:([^\r\n]+))");
	std::regex section_end(R"(section_end:\d+:([^\r\n]+))");
	std::regex job_result(R"(Job (succeeded|failed))");
	std::regex script_line(R"(^\$ (.+))");
	std::regex error_line(R"(^(ERROR|error):?\s*(.+))");
	std::regex warning_line(R"(^(WARNING|warning):?\s*(.+))");
	std::regex exit_code(R"(exit code (\d+))");
	std::regex runner_info(R"(Running with gitlab-runner\s+(\S+))");
	std::regex artifact_upload(R"(Uploading artifacts.*?(\d+) bytes)");
	std::regex git_checkout(R"(Checking out ([a-f0-9]+) as)");

	std::string current_section;
	bool job_failed = false;

	while (std::getline(stream, line)) {
		line_num++;
		std::smatch match;

		// Section start
		if (std::regex_search(line, match, section_start)) {
			current_section = match[1].str();
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Starting section: " + current_section;
			event.tool_name = "gitlab_ci";
			event.category = "gitlab_ci_text";
			event.scope = current_section;
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Section end
		else if (std::regex_search(line, match, section_end)) {
			// Don't emit event for section end, just clear context
			current_section.clear();
		}
		// Job result
		else if (std::regex_search(line, match, job_result)) {
			std::string result = match[1].str();
			job_failed = (result == "failed");

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = job_failed ? "error" : "info";
			event.status = job_failed ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.message = "Job " + result;
			event.tool_name = "gitlab_ci";
			event.category = "gitlab_ci_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
		// Error lines
		else if (std::regex_search(line, match, error_line)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = match[2].str();
			event.tool_name = "gitlab_ci";
			event.category = "gitlab_ci_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_section.empty()) {
				event.scope = current_section;
			}
			events.push_back(event);
		}
		// Warning lines
		else if (std::regex_search(line, match, warning_line)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.severity = "warning";
			event.status = ValidationEventStatus::WARNING;
			event.message = match[2].str();
			event.tool_name = "gitlab_ci";
			event.category = "gitlab_ci_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			if (!current_section.empty()) {
				event.scope = current_section;
			}
			events.push_back(event);
		}
		// Exit code in error context
		else if (std::regex_search(line, match, exit_code)) {
			int code = std::stoi(match[1].str());
			if (code != 0) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::BUILD_ERROR;
				event.severity = "error";
				event.status = ValidationEventStatus::FAIL;
				event.message = "Command exited with code " + match[1].str();
				event.tool_name = "gitlab_ci";
				event.category = "gitlab_ci_text";
				event.log_content = line;
				event.log_line_start = line_num;
				event.log_line_end = line_num;
				if (!current_section.empty()) {
					event.scope = current_section;
				}
				events.push_back(event);
			}
		}
		// Runner version info
		else if (std::regex_search(line, match, runner_info)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "GitLab Runner version: " + match[1].str();
			event.tool_name = "gitlab_ci";
			event.category = "gitlab_ci_text";
			event.log_content = line;
			event.log_line_start = line_num;
			event.log_line_end = line_num;
			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
