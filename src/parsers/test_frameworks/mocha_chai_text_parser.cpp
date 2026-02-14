#include "mocha_chai_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

// Pre-compiled regex patterns for Mocha/Chai parsing (compiled once, reused)
namespace {
static const std::regex RE_TEST_PASSED(R"(\s*✓\s*(.+)\s*\((\d+)ms\))");
static const std::regex RE_TEST_FAILED(R"(\s*✗\s*(.+))");
static const std::regex RE_TEST_PENDING(R"(\s*-\s*(.+)\s*\(pending\))");
static const std::regex RE_TEST_PENDING_SIMPLE(R"(^\s+-\s+(.+)$)");
static const std::regex RE_CONTEXT_START(R"(^\s*([A-Z][A-Za-z0-9\s]+)\s*$)");
static const std::regex RE_NESTED_CONTEXT(R"(^\s{2,}([a-z#][A-Za-z0-9\s#]+)\s*$)");
static const std::regex RE_ERROR_LINE(R"(\s*(Error|AssertionError|TypeError|ReferenceError):\s*(.+))");
static const std::regex RE_FILE_LINE(R"(\s*at\s+Context\.<anonymous>\s+\((.+):(\d+):(\d+)\))");
static const std::regex RE_TEST_STACK(R"(\s*at\s+Test\.Runnable\.run\s+\((.+):(\d+):(\d+)\))");
static const std::regex RE_GENERAL_FILE_LINE(R"(\s*at\s+.+\s+\((.+):(\d+):(\d+)\))");
static const std::regex RE_SUMMARY_LINE(R"(\s*(\d+)\s+passing\s*\(([0-9.]+s?)\))");
static const std::regex RE_FAILING_LINE(R"(\s*(\d+)\s+failing)");
static const std::regex RE_PENDING_LINE(R"(\s*(\d+)\s+pending)");
static const std::regex RE_FAILED_EXAMPLE_START(R"(\s*(\d+)\)\s+(.+?):?\s*$)");
static const std::regex RE_FAILED_EXAMPLE_CONTINUATION(R"(^\s{6,}(.+?):?\s*$)");
static const std::regex RE_EXPECTED_GOT_LINE(R"(\s*\+(.+))");
static const std::regex RE_ACTUAL_LINE(R"(\s*-(.+))");
static const std::regex RE_NUMBERED_FAILED_TEST(R"(\s*(\d+)\)\s+(.+))");
} // anonymous namespace

bool MochaChaiTextParser::canParse(const std::string &content) const {
	// Check for Mocha/Chai text patterns (should be checked before RSpec since they can share similar symbols)
	return (content.find("✓") != std::string::npos || content.find("✗") != std::string::npos) &&
	       (content.find("passing") != std::string::npos || content.find("failing") != std::string::npos) &&
	       (content.find("Context.<anonymous>") != std::string::npos ||
	        content.find("Test.Runnable.run") != std::string::npos ||
	        content.find("AssertionError") != std::string::npos || content.find("at Context") != std::string::npos);
}

std::vector<ValidationEvent> MochaChaiTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 80); // Estimate: ~1 event per 80 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::string current_context;
	std::string current_nested_context;
	std::string current_test_name;
	std::string current_error_message;
	std::string current_file_path;
	int current_line_number = 0;
	int current_column = 0;
	int64_t current_execution_time = 0;
	std::vector<std::string> stack_trace;
	bool in_failure_details = false;
	int32_t failure_start_line = 0;
	std::string accumulated_test_name;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Check for test passed
		if (std::regex_match(line, match, RE_TEST_PASSED)) {
			std::string test_name = match[1].str();
			std::string time_str = match[2].str();
			current_execution_time = std::stoll(time_str);

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "info";
			event.message = "Test passed: " + test_name;
			event.test_name = current_context + " " + current_nested_context + " " + test_name;
			event.status = ValidationEventStatus::PASS;
			event.ref_file = current_file_path;
			event.ref_line = current_line_number;
			event.ref_column = current_column;
			event.execution_time = current_execution_time;
			event.tool_name = "mocha";
			event.category = "mocha_chai_text";
			event.log_content = line;
			event.function_name = current_context;
			event.structured_data = "{}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);

			// Reset for next test
			current_file_path = "";
			current_line_number = 0;
			current_column = 0;
			current_execution_time = 0;
		}
		// Check for test failed
		else if (std::regex_match(line, match, RE_TEST_FAILED)) {
			current_test_name = match[1].str();
		}
		// Check for test pending
		else if (std::regex_match(line, match, RE_TEST_PENDING)) {
			std::string test_name = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "warning";
			event.message = "Test pending: " + test_name;
			event.test_name = current_context + " " + current_nested_context + " " + test_name;
			event.status = ValidationEventStatus::SKIP;
			event.ref_file = "";
			event.ref_line = 0;
			event.ref_column = 0;
			event.execution_time = 0;
			event.tool_name = "mocha";
			event.category = "mocha_chai_text";
			event.log_content = line;
			event.function_name = current_context;
			event.structured_data = "{}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for context start
		else if (std::regex_match(line, match, RE_CONTEXT_START)) {
			current_context = match[1].str();
			current_nested_context = "";
		}
		// Check for nested context
		else if (std::regex_match(line, match, RE_NESTED_CONTEXT)) {
			current_nested_context = match[1].str();
		}
		// Check for error messages
		else if (std::regex_search(line, match, RE_ERROR_LINE)) {
			current_error_message = match[1].str() + ": " + match[2].str();
		}
		// Check for file and line information (Context.<anonymous> format)
		else if (std::regex_match(line, match, RE_FILE_LINE)) {
			current_file_path = match[1].str();
			current_line_number = std::stoi(match[2].str());
			current_column = std::stoi(match[3].str());

			// If we have a failed test from inline ✗ marker, create the event now
			if (!current_test_name.empty() && !current_error_message.empty() && !in_failure_details) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::TEST_RESULT;
				event.severity = "error";
				event.message = current_error_message;
				event.test_name = current_context + " " + current_nested_context + " " + current_test_name;
				event.status = ValidationEventStatus::FAIL;
				event.ref_file = current_file_path;
				event.ref_line = current_line_number;
				event.ref_column = current_column;
				event.execution_time = 0;
				event.tool_name = "mocha";
				event.category = "mocha_chai_text";
				event.log_content = line;
				event.function_name = current_context;
				event.structured_data = "{}";

				events.push_back(event);

				// Reset for next test
				current_test_name = "";
				current_error_message = "";
				current_file_path = "";
				current_line_number = 0;
				current_column = 0;
			}
		}
		// Check for general file and line information (any "at ... (file:line:col)" format)
		else if (in_failure_details && current_file_path.empty() && std::regex_search(line, match, RE_GENERAL_FILE_LINE)) {
			current_file_path = match[1].str();
			current_line_number = std::stoi(match[2].str());
			current_column = std::stoi(match[3].str());
		}
		// Check for failed example start (in failure summary section)
		// Format: "  1) Context name test name:" or "  1) Context name"
		else if (std::regex_match(line, match, RE_FAILED_EXAMPLE_START)) {
			// If we have a pending failure from previous block, emit it
			if (in_failure_details && !accumulated_test_name.empty()) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::TEST_RESULT;
				event.severity = "error";
				event.message = current_error_message.empty() ? "Test failed" : current_error_message;
				event.test_name = accumulated_test_name;
				event.status = ValidationEventStatus::FAIL;
				event.ref_file = current_file_path;
				event.ref_line = current_line_number;
				event.ref_column = current_column;
				event.execution_time = 0;
				event.tool_name = "mocha";
				event.category = "mocha_chai_text";
				event.log_content = "";
				event.function_name = "";
				event.structured_data = "{}";
				event.log_line_start = failure_start_line;
				event.log_line_end = current_line_num - 1;
				events.push_back(event);
			}

			// Start new failure (match[1] contains the failure number)
			accumulated_test_name = match[2].str();
			// Remove trailing colon if present
			if (!accumulated_test_name.empty() && accumulated_test_name.back() == ':') {
				accumulated_test_name.pop_back();
			}
			in_failure_details = true;
			failure_start_line = current_line_num;
			current_error_message = "";
			current_file_path = "";
			current_line_number = 0;
			current_column = 0;
		}
		// Check for continuation lines in failure details (indented test name parts)
		else if (in_failure_details && std::regex_match(line, match, RE_FAILED_EXAMPLE_CONTINUATION)) {
			std::string continuation = match[1].str();
			// Remove trailing colon if present
			if (!continuation.empty() && continuation.back() == ':') {
				continuation.pop_back();
			}
			accumulated_test_name += " " + continuation;
		}
		// Check for summary lines
		else if (std::regex_match(line, match, RE_SUMMARY_LINE)) {
			int passing_count = std::stoi(match[1].str());
			std::string total_time = match[2].str();

			duckdb::ValidationEvent summary_event;
			summary_event.event_id = event_id++;
			summary_event.event_type = ValidationEventType::SUMMARY;
			summary_event.severity = "info";
			summary_event.message = "Test execution completed with " + std::to_string(passing_count) + " passing tests";
			summary_event.test_name = "";
			summary_event.status = ValidationEventStatus::INFO;
			summary_event.ref_file = "";
			summary_event.ref_line = 0;
			summary_event.ref_column = 0;
			summary_event.execution_time = 0;
			summary_event.tool_name = "mocha";
			summary_event.category = "mocha_chai_text";
			summary_event.log_content = line;
			summary_event.function_name = "";
			summary_event.structured_data =
			    "{\"passing_tests\": " + std::to_string(passing_count) + ", \"total_time\": \"" + total_time + "\"}";

			events.push_back(summary_event);
		} else if (std::regex_match(line, match, RE_FAILING_LINE)) {
			int failing_count = std::stoi(match[1].str());

			duckdb::ValidationEvent summary_event;
			summary_event.event_id = event_id++;
			summary_event.event_type = ValidationEventType::SUMMARY;
			summary_event.severity = "error";
			summary_event.message = "Test execution completed with " + std::to_string(failing_count) + " failing tests";
			summary_event.test_name = "";
			summary_event.status = ValidationEventStatus::FAIL;
			summary_event.ref_file = "";
			summary_event.ref_line = 0;
			summary_event.ref_column = 0;
			summary_event.execution_time = 0;
			summary_event.tool_name = "mocha";
			summary_event.category = "mocha_chai_text";
			summary_event.log_content = line;
			summary_event.function_name = "";
			summary_event.structured_data = "{\"failing_tests\": " + std::to_string(failing_count) + "}";

			events.push_back(summary_event);
		} else if (std::regex_match(line, match, RE_PENDING_LINE)) {
			int pending_count = std::stoi(match[1].str());

			duckdb::ValidationEvent summary_event;
			summary_event.event_id = event_id++;
			summary_event.event_type = ValidationEventType::SUMMARY;
			summary_event.severity = "warning";
			summary_event.message = "Test execution completed with " + std::to_string(pending_count) + " pending tests";
			summary_event.test_name = "";
			summary_event.status = ValidationEventStatus::WARNING;
			summary_event.ref_file = "";
			summary_event.ref_line = 0;
			summary_event.ref_column = 0;
			summary_event.execution_time = 0;
			summary_event.tool_name = "mocha";
			summary_event.category = "mocha_chai_text";
			summary_event.log_content = line;
			summary_event.function_name = "";
			summary_event.structured_data = "{\"pending_tests\": " + std::to_string(pending_count) + "}";

			events.push_back(summary_event);
		}

		// Always add stack trace lines when we encounter them
		if (std::regex_match(line, match, RE_TEST_STACK) || std::regex_match(line, match, RE_FILE_LINE)) {
			stack_trace.push_back(line);
		}
	}

	// Emit any pending failure at the end of the file
	if (in_failure_details && !accumulated_test_name.empty()) {
		ValidationEvent event;
		event.event_id = event_id++;
		event.event_type = ValidationEventType::TEST_RESULT;
		event.severity = "error";
		event.message = current_error_message.empty() ? "Test failed" : current_error_message;
		event.test_name = accumulated_test_name;
		event.status = ValidationEventStatus::FAIL;
		event.ref_file = current_file_path;
		event.ref_line = current_line_number;
		event.ref_column = current_column;
		event.execution_time = 0;
		event.tool_name = "mocha";
		event.category = "mocha_chai_text";
		event.log_content = "";
		event.function_name = "";
		event.structured_data = "{}";
		event.log_line_start = failure_start_line;
		event.log_line_end = current_line_num;
		events.push_back(event);
	}

	return events;
}

} // namespace duckdb
