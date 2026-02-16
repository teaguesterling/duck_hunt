#include "junit_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

// Pre-compiled regex patterns for JUnit parsing (compiled once, reused)
namespace {
// JUnit 4 patterns
static const std::regex RE_JUNIT4_CLASS(R"(Running (.+))");
static const std::regex RE_JUNIT4_SUMMARY(
    R"(Tests run: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+), Time elapsed: ([\d.]+) sec.*?)");
static const std::regex RE_JUNIT4_TEST(
    R"((.+?)\((.+?)\)\s+Time elapsed: ([\d.]+) sec\s+<<< (PASSED!|FAILURE!|ERROR!|SKIPPED!))");
static const std::regex RE_JUNIT4_EXCEPTION(R"((.+?): (.+)$)");
static const std::regex RE_JUNIT4_STACK_TRACE(R"(\s+at (.+?)\.(.+?)\((.+?):(\d+)\))");

// JUnit 5 patterns
static const std::regex RE_JUNIT5_HEADER(R"(JUnit Jupiter ([\d.]+))");
static const std::regex RE_JUNIT5_CLASS(R"([├└]─ (.+?) [✓✗↷])");
static const std::regex RE_JUNIT5_TEST(R"([│\s]+[├└]─ (.+?)\(\) ([✓✗↷]) \((\d+)ms\))");
static const std::regex RE_JUNIT5_SUMMARY(R"(\[\s+(\d+) tests (found|successful|failed|skipped)\s+\])");

// Maven Surefire patterns
static const std::regex RE_SUREFIRE_CLASS(R"(\[INFO\] Running (.+))");
static const std::regex RE_SUREFIRE_TEST(R"(\[ERROR\] (.+?)\((.+?)\)\s+Time elapsed: ([\d.]+) s\s+<<< (FAILURE!|ERROR!))");
static const std::regex RE_SUREFIRE_SUMMARY(R"(\[INFO\] Tests run: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+))");
static const std::regex RE_SUREFIRE_RESULTS(R"(\[ERROR\] (.+):(\d+) (.+))");

// Gradle patterns
static const std::regex RE_GRADLE_TEST(R"((.+?) > (.+?) (PASSED|FAILED|SKIPPED))");
static const std::regex RE_GRADLE_SUMMARY(R"((\d+) tests completed, (\d+) failed, (\d+) skipped)");

// TestNG patterns
static const std::regex RE_TESTNG_TEST(R"((.+?)\.(.+?): (PASS|FAIL|SKIP))");
static const std::regex RE_TESTNG_SUMMARY(R"(Total tests run: (\d+), Failures: (\d+), Skips: (\d+))");
} // anonymous namespace

bool JUnitTextParser::canParse(const std::string &content) const {
	// Check for JUnit text patterns (should be checked before pytest since they can contain similar keywords)
	return content.find("Running ") != std::string::npos &&
	       (content.find("Tests run:") != std::string::npos || content.find("JUnit Jupiter") != std::string::npos ||
	        content.find(">>> ") != std::string::npos || content.find("<<< ") != std::string::npos ||
	        content.find("[INFO] Running") != std::string::npos);
}

// Forward declaration for implementation
static void parseJUnitTextImpl(const std::string &content, std::vector<ValidationEvent> &events);

std::vector<ValidationEvent> JUnitTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	parseJUnitTextImpl(content, events);
	return events;
}

static void parseJUnitTextImpl(const std::string &content, std::vector<ValidationEvent> &events) {
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream iss(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::string current_class = "";
	std::string current_exception = "";
	std::string current_test = "";
	bool in_stack_trace = false;

	while (std::getline(iss, line)) {
		current_line_num++;
		std::smatch match;

		// Parse JUnit 4 class execution
		if (std::regex_search(line, match, RE_JUNIT4_CLASS)) {
			current_class = match[1].str();
			in_stack_trace = false;
		}
		// Parse JUnit 4 class summary
		else if (std::regex_search(line, match, RE_JUNIT4_SUMMARY)) {
			int tests_run = SafeParsing::SafeStoi(match[1].str());
			int failures = SafeParsing::SafeStoi(match[2].str());
			int errors = SafeParsing::SafeStoi(match[3].str());
			int skipped = SafeParsing::SafeStoi(match[4].str());
			double time_elapsed = SafeParsing::SafeStod(match[5].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "junit4";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.function_name = current_class;
			event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.severity = (failures > 0 || errors > 0) ? "error" : "info";
			event.category = "test_summary";
			event.message = "Tests: " + std::to_string(tests_run) + " total, " +
			                std::to_string(tests_run - failures - errors - skipped) + " passed, " +
			                std::to_string(failures) + " failed, " + std::to_string(errors) + " errors, " +
			                std::to_string(skipped) + " skipped";
			event.execution_time = time_elapsed;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse JUnit 4 individual test results
		else if (std::regex_search(line, match, RE_JUNIT4_TEST)) {
			std::string test_method = match[1].str();
			std::string test_class = match[2].str();
			double time_elapsed = SafeParsing::SafeStod(match[3].str());
			std::string result = match[4].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "junit4";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.function_name = test_method;
			event.test_name = test_class + "." + test_method;
			event.execution_time = time_elapsed;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			if (result == "PASSED!") {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
				event.category = "test_success";
				event.message = "Test passed";
			} else if (result == "FAILURE!") {
				event.status = ValidationEventStatus::FAIL;
				event.severity = "error";
				event.category = "test_failure";
				event.message = "Test failed";
				current_test = event.test_name;
				in_stack_trace = true;
			} else if (result == "ERROR!") {
				event.status = ValidationEventStatus::ERROR;
				event.severity = "error";
				event.category = "test_error";
				event.message = "Test error";
				current_test = event.test_name;
				in_stack_trace = true;
			} else if (result == "SKIPPED!") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "info";
				event.category = "test_skipped";
				event.message = "Test skipped";
			}
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse JUnit 5 header
		else if (std::regex_search(line, match, RE_JUNIT5_HEADER)) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "junit5";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.category = "test_framework";
			event.message = "JUnit Jupiter " + match[1].str();
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse JUnit 5 class results
		else if (std::regex_search(line, match, RE_JUNIT5_CLASS)) {
			current_class = match[1].str();
		}
		// Parse JUnit 5 test results
		else if (std::regex_search(line, match, RE_JUNIT5_TEST)) {
			std::string test_method = match[1].str();
			std::string result_symbol = match[2].str();
			int time_ms = SafeParsing::SafeStoi(match[3].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "junit5";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.function_name = test_method;
			event.test_name = current_class + "." + test_method;
			event.execution_time = static_cast<double>(time_ms) / 1000.0;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			if (result_symbol == "✓") {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
				event.category = "test_success";
				event.message = "Test passed";
			} else if (result_symbol == "✗") {
				event.status = ValidationEventStatus::FAIL;
				event.severity = "error";
				event.category = "test_failure";
				event.message = "Test failed";
			} else if (result_symbol == "↷") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "info";
				event.category = "test_skipped";
				event.message = "Test skipped";
			}
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse Maven Surefire class execution
		else if (std::regex_search(line, match, RE_SUREFIRE_CLASS)) {
			current_class = match[1].str();
		}
		// Parse Maven Surefire test failures
		else if (std::regex_search(line, match, RE_SUREFIRE_TEST)) {
			std::string test_method = match[1].str();
			std::string test_class = match[2].str();
			double time_elapsed = SafeParsing::SafeStod(match[3].str());
			std::string result = match[4].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "surefire";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.function_name = test_method;
			event.test_name = test_class + "." + test_method;
			event.execution_time = time_elapsed;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			if (result == "FAILURE!") {
				event.status = ValidationEventStatus::FAIL;
				event.severity = "error";
				event.category = "test_failure";
				event.message = "Test failed";
			} else if (result == "ERROR!") {
				event.status = ValidationEventStatus::ERROR;
				event.severity = "error";
				event.category = "test_error";
				event.message = "Test error";
			}
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse Maven Surefire summary
		else if (std::regex_search(line, match, RE_SUREFIRE_SUMMARY)) {
			int tests_run = SafeParsing::SafeStoi(match[1].str());
			int failures = SafeParsing::SafeStoi(match[2].str());
			int errors = SafeParsing::SafeStoi(match[3].str());
			int skipped = SafeParsing::SafeStoi(match[4].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "surefire";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.status = (failures > 0 || errors > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.severity = (failures > 0 || errors > 0) ? "error" : "info";
			event.category = "test_summary";
			event.message = "Tests: " + std::to_string(tests_run) + " total, " +
			                std::to_string(tests_run - failures - errors - skipped) + " passed, " +
			                std::to_string(failures) + " failed, " + std::to_string(errors) + " errors, " +
			                std::to_string(skipped) + " skipped";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse Gradle test results
		else if (std::regex_search(line, match, RE_GRADLE_TEST)) {
			std::string test_class = match[1].str();
			std::string test_method = match[2].str();
			std::string result = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "gradle-test";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.function_name = test_method;
			event.test_name = test_class + "." + test_method;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			if (result == "PASSED") {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
				event.category = "test_success";
				event.message = "Test passed";
			} else if (result == "FAILED") {
				event.status = ValidationEventStatus::FAIL;
				event.severity = "error";
				event.category = "test_failure";
				event.message = "Test failed";
			} else if (result == "SKIPPED") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "info";
				event.category = "test_skipped";
				event.message = "Test skipped";
			}
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse Gradle test summary
		else if (std::regex_search(line, match, RE_GRADLE_SUMMARY)) {
			int total = SafeParsing::SafeStoi(match[1].str());
			int failed = SafeParsing::SafeStoi(match[2].str());
			int skipped = SafeParsing::SafeStoi(match[3].str());
			int passed = total - failed - skipped;

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "gradle-test";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.severity = (failed > 0) ? "error" : "info";
			event.category = "test_summary";
			event.message = "Tests: " + std::to_string(total) + " total, " + std::to_string(passed) + " passed, " +
			                std::to_string(failed) + " failed, " + std::to_string(skipped) + " skipped";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse TestNG test results
		else if (std::regex_search(line, match, RE_TESTNG_TEST)) {
			std::string test_class = match[1].str();
			std::string test_method = match[2].str();
			std::string result = match[3].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "testng";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.function_name = test_method;
			event.test_name = test_class + "." + test_method;
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			if (result == "PASS") {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
				event.category = "test_success";
				event.message = "Test passed";
			} else if (result == "FAIL") {
				event.status = ValidationEventStatus::FAIL;
				event.severity = "error";
				event.category = "test_failure";
				event.message = "Test failed";
			} else if (result == "SKIP") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "info";
				event.category = "test_skipped";
				event.message = "Test skipped";
			}
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Parse TestNG summary
		else if (std::regex_search(line, match, RE_TESTNG_SUMMARY)) {
			int total = SafeParsing::SafeStoi(match[1].str());
			int failed = SafeParsing::SafeStoi(match[2].str());
			int skipped = SafeParsing::SafeStoi(match[3].str());
			int passed = total - failed - skipped;

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "testng";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.status = (failed > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.severity = (failed > 0) ? "error" : "info";
			event.category = "test_summary";
			event.message = "Tests: " + std::to_string(total) + " total, " + std::to_string(passed) + " passed, " +
			                std::to_string(failed) + " failed, " + std::to_string(skipped) + " skipped";
			event.execution_time = 0.0;
			event.log_content = content;
			event.structured_data = "junit";
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Handle exception messages in stack traces
		else if (in_stack_trace) {
			if (std::regex_search(line, match, RE_JUNIT4_EXCEPTION)) {
				current_exception = match[1].str() + ": " + match[2].str();
			} else if (std::regex_search(line, match, RE_JUNIT4_STACK_TRACE)) {
				// Extract file and line info from stack trace
				std::string file = match[3].str();
				int line_number = SafeParsing::SafeStoi(match[4].str());

				// Update the last test event with exception details
				if (!events.empty() && events.back().test_name == current_test) {
					ValidationEvent &last_event = events.back();
					last_event.ref_file = file;
					last_event.ref_line = line_number;
					if (!current_exception.empty()) {
						last_event.message = current_exception;
					}
				}
			}
		}
	}
}

} // namespace duckdb
