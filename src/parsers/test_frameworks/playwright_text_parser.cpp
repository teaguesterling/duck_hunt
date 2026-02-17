#include "playwright_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

// Pre-compiled regex patterns for Playwright text parsing (compiled once, reused)
namespace {
static const std::regex
    RE_TEST_PASSED(R"(\s*✓\s+\d+\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+?)\s+\((\d+)m?s\))");
static const std::regex
    RE_TEST_FAILED(R"(\s*[✘×]\s+\d+\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+?)\s+\((\d+)m?s\))");
static const std::regex RE_TEST_SKIPPED(R"(\s*-\s+\d+\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+))");
static const std::regex RE_PROGRESS_LINE(R"(\[\d+/\d+\]\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+))");
static const std::regex RE_FAILURE_HEADER(R"(\s*(\d+)\)\s+\[(\w+)\]\s+›\s+([^›]+):(\d+):(\d+)\s+›\s+(.+?)\s*─*)");
static const std::regex RE_ERROR_LINE(R"(\s*(Error|AssertionError|TypeError|TimeoutError):\s*(.+))");
static const std::regex RE_STACK_LINE(R"(\s*at\s+(.+):(\d+):(\d+))");
static const std::regex RE_PASSED_SUMMARY(R"(\s*(\d+)\s+passed\s*\(([^)]+)\))");
static const std::regex RE_FAILED_SUMMARY(R"(\s*(\d+)\s+failed)");
static const std::regex RE_SKIPPED_SUMMARY(R"(\s*(\d+)\s+skipped)");
static const std::regex RE_FLAKY_SUMMARY(R"(\s*(\d+)\s+flaky)");
} // anonymous namespace

bool PlaywrightTextParser::canParse(const std::string &content) const {
	// Check for Playwright-specific patterns
	// Must have test count pattern AND either checkmarks or browser markers
	bool has_running_tests = content.find("Running ") != std::string::npos &&
	                         content.find(" tests using ") != std::string::npos &&
	                         content.find(" worker") != std::string::npos;

	bool has_browser_marker = content.find("[chromium]") != std::string::npos ||
	                          content.find("[firefox]") != std::string::npos ||
	                          content.find("[webkit]") != std::string::npos;

	bool has_file_marker =
	    content.find(".spec.js:") != std::string::npos || content.find(".spec.ts:") != std::string::npos ||
	    content.find(".test.js:") != std::string::npos || content.find(".test.ts:") != std::string::npos;

	// Must have running tests header OR (browser marker AND file marker)
	return has_running_tests || (has_browser_marker && has_file_marker);
}

std::vector<ValidationEvent> PlaywrightTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

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
		if (std::regex_search(clean_line, match, RE_TEST_PASSED)) {
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
			current_file_line = SafeParsing::SafeStoi(match[3].str());
			current_file_col = SafeParsing::SafeStoi(match[4].str());
			current_test_name = match[5].str();
			int duration_ms = SafeParsing::SafeStoi(match[6].str());

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
		else if (std::regex_search(clean_line, match, RE_TEST_FAILED)) {
			current_browser = match[1].str();
			current_file = match[2].str();
			current_file_line = SafeParsing::SafeStoi(match[3].str());
			current_file_col = SafeParsing::SafeStoi(match[4].str());
			current_test_name = match[5].str();
			// Don't emit yet - wait for error details
		}
		// Check for skipped test
		else if (std::regex_search(clean_line, match, RE_TEST_SKIPPED)) {
			current_browser = match[1].str();
			current_file = match[2].str();
			current_file_line = SafeParsing::SafeStoi(match[3].str());
			current_file_col = SafeParsing::SafeStoi(match[4].str());
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
		else if (std::regex_search(clean_line, match, RE_FAILURE_HEADER)) {
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
			current_file_line = SafeParsing::SafeStoi(match[4].str());
			current_file_col = SafeParsing::SafeStoi(match[5].str());
			current_test_name = match[6].str();
			current_error_message.clear();
			error_file.clear();
			error_line_num = 0;
			error_col = 0;
		}
		// Check for error message
		else if (in_failure_block && std::regex_search(clean_line, match, RE_ERROR_LINE)) {
			current_error_message = match[1].str() + ": " + match[2].str();
		}
		// Check for stack trace (get first one for error location)
		else if (in_failure_block && error_file.empty() && std::regex_search(clean_line, match, RE_STACK_LINE)) {
			error_file = match[1].str();
			error_line_num = SafeParsing::SafeStoi(match[2].str());
			error_col = SafeParsing::SafeStoi(match[3].str());
		}
		// Summary: passed
		else if (std::regex_search(clean_line, match, RE_PASSED_SUMMARY)) {
			int passed_count = SafeParsing::SafeStoi(match[1].str());
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
			event.structured_data =
			    "{\"passed\": " + std::to_string(passed_count) + ", \"duration\": \"" + duration + "\"}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Summary: failed
		else if (std::regex_search(clean_line, match, RE_FAILED_SUMMARY)) {
			int failed_count = SafeParsing::SafeStoi(match[1].str());

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
		else if (std::regex_search(clean_line, match, RE_SKIPPED_SUMMARY)) {
			int skipped_count = SafeParsing::SafeStoi(match[1].str());

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
