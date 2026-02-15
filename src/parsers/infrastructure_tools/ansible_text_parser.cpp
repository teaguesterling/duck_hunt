#include "ansible_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>

namespace duckdb {

// Pre-compiled regex patterns for Ansible text parsing (compiled once, reused)
namespace {
static const std::regex RE_PLAY_START(R"(PLAY \[([^\]]+)\] \*+)");
static const std::regex RE_TASK_START(R"(TASK \[([^\]]+)\] \*+)");
static const std::regex RE_TASK_OK(R"(ok: \[([^\]]+)\]( => \((.+)\))?)");
static const std::regex RE_TASK_CHANGED(R"(changed: \[([^\]]+)\]( => \((.+)\))?)");
static const std::regex RE_TASK_SKIPPING(R"(skipping: \[([^\]]+)\]( => \((.+)\))?)");
static const std::regex RE_TASK_FAILED(R"(fatal: \[([^\]]+)\]: FAILED! => (.+))");
static const std::regex RE_TASK_UNREACHABLE(R"(fatal: \[([^\]]+)\]: UNREACHABLE! => (.+))");
static const std::regex RE_HANDLER_RUNNING(R"(RUNNING HANDLER \[([^\]]+)\] \*+)");
static const std::regex RE_PLAY_RECAP_START(R"(PLAY RECAP \*+)");
static const std::regex RE_PLAY_RECAP_HOST(
    R"((\S+)\s+:\s+ok=(\d+)\s+changed=(\d+)\s+unreachable=(\d+)\s+failed=(\d+)\s+skipped=(\d+)\s+rescued=(\d+)\s+ignored=(\d+))");
static const std::regex RE_ANSIBLE_ERROR(R"(ERROR! (.+))");
static const std::regex RE_ANSIBLE_WARNING(R"(\[WARNING\]: (.+))");
static const std::regex RE_DEPRECATION_WARNING(R"(\[DEPRECATION WARNING\]: (.+))");
static const std::regex RE_RETRY_FAILED(R"(FAILED - RETRYING: (.+) \((\d+) retries left\))");
static const std::regex RE_TASK_RETRY_EXHAUSTED(R"(fatal: \[([^\]]+)\]: FAILED! => \{\"attempts\": (\d+), .+\"msg\": \"(.+)\"\})");
static const std::regex RE_CONFIG_DIFF(R"(--- (.+))");
static const std::regex RE_ANSIBLE_NOTIFIED(R"(NOTIFIED: \[([^\]]+)\] \*+)");
} // anonymous namespace

bool AnsibleTextParser::canParse(const std::string &content) const {
	// Check for Ansible-specific patterns
	return content.find("PLAY [") != std::string::npos || content.find("TASK [") != std::string::npos ||
	       content.find("PLAY RECAP") != std::string::npos || content.find("ok: [") != std::string::npos ||
	       content.find("changed: [") != std::string::npos || content.find("FAILED!") != std::string::npos;
}

std::vector<ValidationEvent> AnsibleTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	std::smatch match;

	bool in_play_recap = false;
	std::string current_play = "";
	std::string current_task = "";

	while (std::getline(stream, line)) {
		// Parse play start
		if (std::regex_search(line, match, RE_PLAY_START)) {
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
		if (std::regex_search(line, match, RE_TASK_START)) {
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
		if (std::regex_search(line, match, RE_TASK_OK)) {
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
		if (std::regex_search(line, match, RE_TASK_CHANGED)) {
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
		if (std::regex_search(line, match, RE_TASK_SKIPPING)) {
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
		if (std::regex_search(line, match, RE_TASK_FAILED)) {
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
		if (std::regex_search(line, match, RE_TASK_UNREACHABLE)) {
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
		if (std::regex_search(line, match, RE_HANDLER_RUNNING)) {
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
		if (std::regex_match(line, RE_PLAY_RECAP_START)) {
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
		if (in_play_recap && std::regex_search(line, match, RE_PLAY_RECAP_HOST)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "ansible";
			event.event_type = ValidationEventType::SUMMARY;
			event.ref_file = "";
			event.ref_line = -1;
			event.ref_column = -1;

			// Determine overall status based on failed/unreachable counts
			int failed = SafeParsing::SafeStoi(match[5].str());
			int unreachable = SafeParsing::SafeStoi(match[4].str());
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
		if (std::regex_search(line, match, RE_ANSIBLE_ERROR)) {
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
		if (std::regex_search(line, match, RE_ANSIBLE_WARNING)) {
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
		if (std::regex_search(line, match, RE_DEPRECATION_WARNING)) {
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
		if (std::regex_search(line, match, RE_RETRY_FAILED)) {
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
		if (std::regex_search(line, match, RE_TASK_RETRY_EXHAUSTED)) {
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
		if (std::regex_search(line, match, RE_CONFIG_DIFF)) {
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
		if (std::regex_search(line, match, RE_ANSIBLE_NOTIFIED)) {
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
