#include "gtest_text_parser.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

// Pre-compiled regex patterns for Google Test parsing (compiled once, reused)
namespace {
static const std::regex RE_TEST_RUN_START(R"(\[\s*RUN\s*\]\s*(.+))");
static const std::regex RE_TEST_PASSED(R"(\[\s*OK\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
static const std::regex RE_TEST_FAILED(R"(\[\s*FAILED\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
static const std::regex RE_TEST_SKIPPED(R"(\[\s*SKIPPED\s*\]\s*(.+)\s*\((\d+)\s*ms\))");
static const std::regex RE_TEST_SUITE_START(R"(\[----------\]\s*(\d+)\s*tests from\s*(.+))");
static const std::regex RE_TEST_SUITE_END(R"(\[----------\]\s*(\d+)\s*tests from\s*(.+)\s*\((\d+)\s*ms total\))");
static const std::regex RE_TEST_SUMMARY_START(
    R"(\[==========\]\s*(\d+)\s*tests from\s*(\d+)\s*test suites ran\.\s*\((\d+)\s*ms total\))");
static const std::regex RE_TESTS_PASSED_SUMMARY(R"(\[\s*PASSED\s*\]\s*(\d+)\s*tests\.)");
static const std::regex RE_TESTS_FAILED_SUMMARY(R"(\[\s*FAILED\s*\]\s*(\d+)\s*tests,\s*listed below:)");
static const std::regex RE_FAILED_TEST_LIST(R"(\[\s*FAILED\s*\]\s*(.+))");
static const std::regex RE_FAILURE_DETAIL(R"((.+):\s*(.+):(\d+):\s*Failure)");
static const std::regex RE_GLOBAL_ENV_SETUP(R"(\[----------\]\s*Global test environment set-up)");
static const std::regex RE_GLOBAL_ENV_TEARDOWN(R"(\[----------\]\s*Global test environment tear-down)");
} // anonymous namespace

bool GTestTextParser::canParse(const std::string &content) const {
	return content.find("[RUN      ]") != std::string::npos || content.find("[       OK ]") != std::string::npos ||
	       content.find("[  FAILED  ]") != std::string::npos || content.find("[==========]") != std::string::npos ||
	       content.find("[----------]") != std::string::npos;
}

std::vector<ValidationEvent> GTestTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::string current_test_suite;
	std::string current_test_name;
	std::vector<std::string> failure_lines;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Check for test run start
		if (std::regex_match(line, match, RE_TEST_RUN_START)) {
			current_test_name = match[1].str();
		}
		// Check for test passed
		else if (std::regex_match(line, match, RE_TEST_PASSED)) {
			std::string test_name = match[1].str();
			std::string time_str = match[2].str();
			int64_t execution_time = std::stoll(time_str);

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "info";
			event.message = "Test passed: " + test_name;
			event.test_name = test_name;
			event.status = ValidationEventStatus::PASS;
			event.ref_file = "";
			event.ref_line = 0;
			event.ref_column = 0;
			event.execution_time = execution_time;
			event.tool_name = "gtest";
			event.category = "gtest_text";
			event.log_content = line;
			event.function_name = current_test_suite;
			event.structured_data = "{}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for test failed
		else if (std::regex_match(line, match, RE_TEST_FAILED)) {
			std::string test_name = match[1].str();
			std::string time_str = match[2].str();
			int64_t execution_time = std::stoll(time_str);

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "error";
			event.message = "Test failed: " + test_name;
			event.test_name = test_name;
			event.status = ValidationEventStatus::FAIL;
			event.ref_file = "";
			event.ref_line = 0;
			event.ref_column = 0;
			event.execution_time = execution_time;
			event.tool_name = "gtest";
			event.category = "gtest_text";
			event.log_content = line;
			event.function_name = current_test_suite;
			event.structured_data = "{}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for test skipped
		else if (std::regex_match(line, match, RE_TEST_SKIPPED)) {
			std::string test_name = match[1].str();
			std::string time_str = match[2].str();
			int64_t execution_time = std::stoll(time_str);

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "warning";
			event.message = "Test skipped: " + test_name;
			event.test_name = test_name;
			event.status = ValidationEventStatus::SKIP;
			event.ref_file = "";
			event.ref_line = 0;
			event.ref_column = 0;
			event.execution_time = execution_time;
			event.tool_name = "gtest";
			event.category = "gtest_text";
			event.log_content = line;
			event.function_name = current_test_suite;
			event.structured_data = "{}";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Check for test suite start
		else if (std::regex_match(line, match, RE_TEST_SUITE_START)) {
			current_test_suite = match[2].str();
		}
		// Check for test suite end
		else if (std::regex_match(line, match, RE_TEST_SUITE_END)) {
			std::string suite_name = match[2].str();
			std::string total_time = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.message = "Test suite completed: " + suite_name + " (" + total_time + " ms total)";
			event.test_name = "";
			event.status = ValidationEventStatus::INFO;
			event.ref_file = "";
			event.ref_line = 0;
			event.ref_column = 0;
			event.execution_time = std::stoll(total_time);
			event.tool_name = "gtest";
			event.category = "gtest_text";
			event.log_content = line;
			event.function_name = suite_name;
			event.structured_data = "{\"suite_name\": \"" + suite_name + "\", \"total_time_ms\": " + total_time + "}";

			events.push_back(event);
		}
		// Check for overall test summary
		else if (std::regex_match(line, match, RE_TEST_SUMMARY_START)) {
			std::string total_tests = match[1].str();
			std::string total_suites = match[2].str();
			std::string total_time = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.message = "Test execution completed: " + total_tests + " tests from " + total_suites + " test suites";
			event.test_name = "";
			event.status = ValidationEventStatus::INFO;
			event.ref_file = "";
			event.ref_line = 0;
			event.ref_column = 0;
			event.execution_time = std::stoll(total_time);
			event.tool_name = "gtest";
			event.category = "gtest_text";
			event.log_content = line;
			event.function_name = "";
			event.structured_data = "{\"total_tests\": " + total_tests + ", \"total_suites\": " + total_suites +
			                        ", \"total_time_ms\": " + total_time + "}";

			events.push_back(event);
		}
		// Check for passed tests summary
		else if (std::regex_match(line, match, RE_TESTS_PASSED_SUMMARY)) {
			std::string passed_count = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.message = "Tests passed: " + passed_count + " tests";
			event.test_name = "";
			event.status = ValidationEventStatus::PASS;
			event.ref_file = "";
			event.ref_line = 0;
			event.ref_column = 0;
			event.execution_time = 0;
			event.tool_name = "gtest";
			event.category = "gtest_text";
			event.log_content = line;
			event.function_name = "";
			event.structured_data = "{\"passed_tests\": " + passed_count + "}";

			events.push_back(event);
		}
		// Check for failed tests summary
		else if (std::regex_match(line, match, RE_TESTS_FAILED_SUMMARY)) {
			std::string failed_count = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "error";
			event.message = "Tests failed: " + failed_count + " tests";
			event.test_name = "";
			event.status = ValidationEventStatus::FAIL;
			event.ref_file = "";
			event.ref_line = 0;
			event.ref_column = 0;
			event.execution_time = 0;
			event.tool_name = "gtest";
			event.category = "gtest_text";
			event.log_content = line;
			event.function_name = "";
			event.structured_data = "{\"failed_tests\": " + failed_count + "}";

			events.push_back(event);
		}
		// Check for failure details (file paths and line numbers)
		else if (std::regex_match(line, match, RE_FAILURE_DETAIL)) {
			std::string test_name = match[1].str();
			std::string file_path = match[2].str();
			std::string line_str = match[3].str();
			int line_number = 0;

			try {
				line_number = std::stoi(line_str);
			} catch (...) {
				// If parsing line number fails, keep it as 0
			}

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "error";
			event.message = "Test failure details: " + test_name;
			event.test_name = test_name;
			event.status = ValidationEventStatus::FAIL;
			event.ref_file = file_path;
			event.ref_line = line_number;
			event.ref_column = 0;
			event.execution_time = 0;
			event.tool_name = "gtest";
			event.category = "gtest_text";
			event.log_content = line;
			event.function_name = current_test_suite;
			event.structured_data =
			    "{\"file_path\": \"" + file_path + "\", \"line_number\": " + std::to_string(line_number) + "}";

			events.push_back(event);
		}
	}

	return events;
}

} // namespace duckdb
