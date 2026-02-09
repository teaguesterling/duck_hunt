#include "playwright_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

bool PlaywrightTextParser::canParse(const std::string &content) const {
	// Check for Playwright-specific patterns
	// Must have test count pattern AND either checkmarks or browser markers
	bool has_running_tests = content.find("Running ") != std::string::npos &&
	                         content.find(" tests using ") != std::string::npos &&
	                         content.find(" worker") != std::string::npos;

	bool has_browser_marker = content.find("[chromium]") != std::string::npos ||
	                          content.find("[firefox]") != std::string::npos ||
	                          content.find("[webkit]") != std::string::npos;

	bool has_file_marker = content.find(".spec.js:") != std::string::npos ||
	                       content.find(".spec.ts:") != std::string::npos ||
	                       content.find(".test.js:") != std::string::npos ||
	                       content.find(".test.ts:") != std::string::npos;

	// Must have running tests header OR (browser marker AND file marker)
	return has_running_tests || (has_browser_marker && has_file_marker);
}

std::vector<ValidationEvent> PlaywrightTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	// Regex patterns for Playwright output
	// Test result: "  ✓  1 [chromium] › tests/file.spec.js:228:3 › Suite › test name (929ms)"
	// Also handles ANSI escape codes that might be present
	std::regex test_passed(R"(\s*✓\s+\d+\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+?)\s+\((\d+)m?s\))");
	std::regex test_failed(R"(\s*[✘×]\s+\d+\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+?)\s+\((\d+)m?s\))");
	std::regex test_skipped(R"(\s*-\s+\d+\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+))");

	// Progress line (during execution): "[1/46] [chromium] › tests/file.spec.js:228:3 › Suite › test"
	std::regex progress_line(R"(\[\d+/\d+\]\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+))");

	// Failure detail header: "  1) [chromium] › tests/fail.spec.js:1:50 › test name ────"
	std::regex failure_header(R"(\s*(\d+)\)\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+?)\s*─*)");

	// Error line: "    Error: expect(received).toBe(expected)"
	std::regex error_line(R"(\s*(Error|AssertionError|TypeError|TimeoutError):\s*(.+))");

	// Stack trace line: "    at /path/to/file.spec.js:1:102"
	std::regex stack_line(R"(\s*at\s+(.+):(\d+):(\d+))");

	// Summary lines
	std::regex passed_summary(R"(\s*(\d+)\s+passed\s*\(([^)]+)\))");
	std::regex failed_summary(R"(\s*(\d+)\s+failed)");
	std::regex skipped_summary(R"(\s*(\d+)\s+skipped)");
	std::regex flaky_summary(R"(\s*(\d+)\s+flaky)");

	// State tracking
	std::string current_browser;
	std::string current_file;
	int current_file_line = 0;
	int current_file_col = 0;
	std::string current_test_name;
	std::string current_error_message;
	std::string error_file;
	int error_line_num = 0;
	int error_col = 0;
	bool in_failure_block = false;
	int32_t failure_start_line = 0;

	while (std::getline(stream, line)) {
		current_line_num++;

		// Skip ANSI escape sequences for matching (but keep original line for log_content)
		std::string clean_line = line;
		// Simple ANSI removal: \x1b[...m patterns
		size_t pos;
		while ((pos = clean_line.find("\x1b[")) != std::string::npos) {
			size_t end = clean_line.find('m', pos);
			if (end != std::string::npos) {
				clean_line.erase(pos, end - pos + 1);
			} else {
				break;
			}
		}
		// Also handle [1A[2K cursor control sequences
		while ((pos = clean_line.find("[1A[2K")) != std::string::npos) {
			clean_line.erase(pos, 6);
		}

		std::smatch match;

		// Check for passed test
		if (std::regex_search(clean_line, match, test_passed)) {
			// Emit any pending failure first
			if (in_failure_block && !current_test_name.empty()) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::TEST_RESULT;
				event.severity = "error";
				event.message = current_error_message.empty() ? "Test failed" : current_error_message;
				event.test_name = current_test_name;
				event.status = ValidationEventStatus::FAIL;
				event.ref_file = error_file.empty() ? current_file : error_file;
				event.ref_line = error_line_num > 0 ? error_line_num : current_file_line;
				event.ref_column = error_col > 0 ? error_col : current_file_col;
				event.execution_time = 0;
				event.tool_name = "playwright";
				event.category = "playwright_text";
				event.structured_data = "{\"browser\": \"" + current_browser + "\"}";
				event.log_line_start = failure_start_line;
				event.log_line_end = current_line_num - 1;
				events.push_back(event);
				in_failure_block = false;
			}

			current_browser = match[1].str();
			current_file = match[2].str();
			current_file_line = std::stoi(match[3].str());
			current_file_col = std::stoi(match[4].str());
			current_test_name = match[5].str();
			int duration_ms = std::stoi(match[6].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "info";
			event.message = "Test passed: " + current_test_name;
			event.test_name = current_test_name;
			event.status = ValidationEventStatus::PASS;
			event.ref_file = current_file;
			event.ref_line = current_file_line;
			event.ref_column = current_file_col;
			event.execution_time = duration_ms;
			event.tool_name = "playwright";
			event.category = "playwright_text";
			event.log_content = line;
			event.structured_data = "{\"browser\": \"" + current_browser + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for failed test marker
		else if (std::regex_search(clean_line, match, test_failed)) {
			current_browser = match[1].str();
			current_file = match[2].str();
			current_file_line = std::stoi(match[3].str());
			current_file_col = std::stoi(match[4].str());
			current_test_name = match[5].str();
			// Don't emit yet - wait for error details
		}
		// Check for skipped test
		else if (std::regex_search(clean_line, match, test_skipped)) {
			current_browser = match[1].str();
			current_file = match[2].str();
			current_file_line = std::stoi(match[3].str());
			current_file_col = std::stoi(match[4].str());
			current_test_name = match[5].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "warning";
			event.message = "Test skipped: " + current_test_name;
			event.test_name = current_test_name;
			event.status = ValidationEventStatus::SKIP;
			event.ref_file = current_file;
			event.ref_line = current_file_line;
			event.ref_column = current_file_col;
			event.execution_time = 0;
			event.tool_name = "playwright";
			event.category = "playwright_text";
			event.log_content = line;
			event.structured_data = "{\"browser\": \"" + current_browser + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for failure detail header
		else if (std::regex_search(clean_line, match, failure_header)) {
			// Emit any pending failure first
			if (in_failure_block && !current_test_name.empty()) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::TEST_RESULT;
				event.severity = "error";
				event.message = current_error_message.empty() ? "Test failed" : current_error_message;
				event.test_name = current_test_name;
				event.status = ValidationEventStatus::FAIL;
				event.ref_file = error_file.empty() ? current_file : error_file;
				event.ref_line = error_line_num > 0 ? error_line_num : current_file_line;
				event.ref_column = error_col > 0 ? error_col : current_file_col;
				event.execution_time = 0;
				event.tool_name = "playwright";
				event.category = "playwright_text";
				event.structured_data = "{\"browser\": \"" + current_browser + "\"}";
				event.log_line_start = failure_start_line;
				event.log_line_end = current_line_num - 1;
				events.push_back(event);
			}

			// Start new failure block
			in_failure_block = true;
			failure_start_line = current_line_num;
			current_browser = match[2].str();
			current_file = match[3].str();
			current_file_line = std::stoi(match[4].str());
			current_file_col = std::stoi(match[5].str());
			current_test_name = match[6].str();
			current_error_message.clear();
			error_file.clear();
			error_line_num = 0;
			error_col = 0;
		}
		// Check for error message
		else if (in_failure_block && std::regex_search(clean_line, match, error_line)) {
			current_error_message = match[1].str() + ": " + match[2].str();
		}
		// Check for stack trace (get first one for error location)
		else if (in_failure_block && error_file.empty() && std::regex_search(clean_line, match, stack_line)) {
			error_file = match[1].str();
			error_line_num = std::stoi(match[2].str());
			error_col = std::stoi(match[3].str());
		}
		// Summary: passed
		else if (std::regex_search(clean_line, match, passed_summary)) {
			int passed_count = std::stoi(match[1].str());
			std::string duration = match[2].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.message = std::to_string(passed_count) + " tests passed";
			event.status = ValidationEventStatus::INFO;
			event.tool_name = "playwright";
			event.category = "playwright_text";
			event.log_content = line;
			event.structured_data = "{\"passed\": " + std::to_string(passed_count) + ", \"duration\": \"" + duration + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Summary: failed
		else if (std::regex_search(clean_line, match, failed_summary)) {
			int failed_count = std::stoi(match[1].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "error";
			event.message = std::to_string(failed_count) + " tests failed";
			event.status = ValidationEventStatus::FAIL;
			event.tool_name = "playwright";
			event.category = "playwright_text";
			event.log_content = line;
			event.structured_data = "{\"failed\": " + std::to_string(failed_count) + "}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Summary: skipped
		else if (std::regex_search(clean_line, match, skipped_summary)) {
			int skipped_count = std::stoi(match[1].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "warning";
			event.message = std::to_string(skipped_count) + " tests skipped";
			event.status = ValidationEventStatus::SKIP;
			event.tool_name = "playwright";
			event.category = "playwright_text";
			event.log_content = line;
			event.structured_data = "{\"skipped\": " + std::to_string(skipped_count) + "}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
	}

	// Emit any pending failure at end of file
	if (in_failure_block && !current_test_name.empty()) {
		ValidationEvent event;
		event.event_id = event_id++;
		event.event_type = ValidationEventType::TEST_RESULT;
		event.severity = "error";
		event.message = current_error_message.empty() ? "Test failed" : current_error_message;
		event.test_name = current_test_name;
		event.status = ValidationEventStatus::FAIL;
		event.ref_file = error_file.empty() ? current_file : error_file;
		event.ref_line = error_line_num > 0 ? error_line_num : current_file_line;
		event.ref_column = error_col > 0 ? error_col : current_file_col;
		event.execution_time = 0;
		event.tool_name = "playwright";
		event.category = "playwright_text";
		event.structured_data = "{\"browser\": \"" + current_browser + "\"}";
		event.log_line_start = failure_start_line;
		event.log_line_end = current_line_num;
		events.push_back(event);
	}

	return events;
}

} // namespace duckdb
