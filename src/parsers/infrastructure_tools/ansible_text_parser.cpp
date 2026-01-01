#include "ansible_text_parser.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

bool AnsibleTextParser::canParse(const std::string &content) const {
	// Check for Ansible-specific patterns
	return content.find("PLAY [") != std::string::npos || content.find("TASK [") != std::string::npos ||
	       content.find("PLAY RECAP") != std::string::npos || content.find("ok: [") != std::string::npos ||
	       content.find("changed: [") != std::string::npos || content.find("FAILED!") != std::string::npos;
}

std::vector<ValidationEvent> AnsibleTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	std::smatch match;

	// Ansible regex patterns
	std::regex play_start(R"(PLAY \[([^\]]+)\] \*+)");
	std::regex task_start(R"(TASK \[([^\]]+)\] \*+)");
	std::regex task_ok(R"(ok: \[([^\]]+)\]( => \((.+)\))?)");
	std::regex task_changed(R"(changed: \[([^\]]+)\]( => \((.+)\))?)");
	std::regex task_skipping(R"(skipping: \[([^\]]+)\]( => \((.+)\))?)");
	std::regex task_failed(R"(fatal: \[([^\]]+)\]: FAILED! => (.+))");
	std::regex task_unreachable(R"(fatal: \[([^\]]+)\]: UNREACHABLE! => (.+))");
	std::regex handler_running(R"(RUNNING HANDLER \[([^\]]+)\] \*+)");
	std::regex play_recap_start(R"(PLAY RECAP \*+)");
	std::regex play_recap_host(
	    R"((\S+)\s+:\s+ok=(\d+)\s+changed=(\d+)\s+unreachable=(\d+)\s+failed=(\d+)\s+skipped=(\d+)\s+rescued=(\d+)\s+ignored=(\d+))");
	std::regex ansible_error(R"(ERROR! (.+))");
	std::regex ansible_warning(R"(\[WARNING\]: (.+))");
	std::regex deprecation_warning(R"(\[DEPRECATION WARNING\]: (.+))");
	std::regex retry_failed(R"(FAILED - RETRYING: (.+) \((\d+) retries left\))");
	std::regex task_retry_exhausted(R"(fatal: \[([^\]]+)\]: FAILED! => \{\"attempts\": (\d+), .+\"msg\": \"(.+)\"\})");
	std::regex config_diff(R"(--- (.+))");
	std::regex ansible_notified(R"(NOTIFIED: \[([^\]]+)\] \*+)");

	bool in_play_recap = false;
	std::string current_play = "";
	std::string current_task = "";

	while (std::getline(stream, line)) {
		// Parse play start
		if (std::regex_search(line, match, play_start)) {
			current_play = match[1].str();
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "play_start";
			event.message = "Starting play: " + current_play;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse task start
		if (std::regex_search(line, match, task_start)) {
			current_task = match[1].str();
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "task_start";
			event.message = "Starting task: " + current_task;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse task OK
		if (std::regex_search(line, match, task_ok)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "task_success";
			event.message = "Task succeeded on " + match[1].str();
			if (match[3].matched) {
				event.suggestion = match[3].str();
			}
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse task changed
		if (std::regex_search(line, match, task_changed)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.category = "task_changed";
			event.message = "Task changed on " + match[1].str();
			if (match[3].matched) {
				event.suggestion = match[3].str();
			}
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse task skipping
		if (std::regex_search(line, match, task_skipping)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::SKIP;
			event.severity = "info";
			event.category = "task_skipped";
			event.message = "Task skipped on " + match[1].str();
			if (match[3].matched) {
				event.suggestion = match[3].str();
			}
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse task failed
		if (std::regex_search(line, match, task_failed)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "task_failed";
			event.message = "Task failed on " + match[1].str() + ": " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse task unreachable
		if (std::regex_search(line, match, task_unreachable)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "host_unreachable";
			event.message = "Host unreachable " + match[1].str() + ": " + match[2].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse handler running
		if (std::regex_search(line, match, handler_running)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "handler";
			event.message = "Running handler: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse play recap start
		if (std::regex_match(line, play_recap_start)) {
			in_play_recap = true;
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "play_recap";
			event.message = "Play recap";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse play recap host summary
		if (in_play_recap && std::regex_search(line, match, play_recap_host)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

			// Determine overall status based on failed/unreachable counts
			int failed = std::stoi(match[5].str());
			int unreachable = std::stoi(match[4].str());
			if (failed > 0 || unreachable > 0) {
				event.status = ValidationEventStatus::ERROR;
				event.severity = "error";
			} else {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
			}

			event.category = "host_summary";
			event.message = "Host " + match[1].str() + " summary: ok=" + match[2].str() + " changed=" + match[3].str() +
			                " unreachable=" + match[4].str() + " failed=" + match[5].str() +
			                " skipped=" + match[6].str() + " rescued=" + match[7].str() + " ignored=" + match[8].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse ansible errors
		if (std::regex_search(line, match, ansible_error)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "ansible_error";
			event.message = match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse ansible warnings
		if (std::regex_search(line, match, ansible_warning)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "ansible_warning";
			event.message = match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse deprecation warnings
		if (std::regex_search(line, match, deprecation_warning)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "deprecation";
			event.message = match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse retry failures
		if (std::regex_search(line, match, retry_failed)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::LINT_ISSUE;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::WARNING;
			event.severity = "warning";
			event.category = "retry";
			event.message = "Retrying: " + match[1].str() + " (" + match[2].str() + " retries left)";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse task retry exhausted
		if (std::regex_search(line, match, task_retry_exhausted)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::BUILD_ERROR;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "retry_exhausted";
			event.message =
			    "Retry exhausted on " + match[1].str() + " after " + match[2].str() + " attempts: " + match[3].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse config diff
		if (std::regex_search(line, match, config_diff)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "config_diff";
			event.message = "Configuration diff: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}

		// Parse ansible notifications
		if (std::regex_search(line, match, ansible_notified)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "notification";
			event.message = "Notified: " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "ansible_text";

			events.push_back(event);
			continue;
		}
	}

	return events;
}

} // namespace duckdb
