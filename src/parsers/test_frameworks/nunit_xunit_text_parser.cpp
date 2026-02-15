#include "nunit_xunit_text_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include <regex>
#include <sstream>
#include <string>

namespace duckdb {

// Pre-compiled regex patterns for NUnit/xUnit parsing (compiled once, reused)
namespace {
// NUnit patterns
static const std::regex RE_NUNIT_HEADER(R"(NUnit\s+([\d\.]+))");
static const std::regex RE_NUNIT_SUMMARY(
    R"(Test Count:\s*(\d+),\s*Passed:\s*(\d+),\s*Failed:\s*(\d+),\s*Warnings:\s*(\d+),\s*Inconclusive:\s*(\d+),\s*Skipped:\s*(\d+))");
static const std::regex RE_NUNIT_OVERALL_RESULT(R"(Overall result:\s*(\w+))");
static const std::regex RE_NUNIT_DURATION(R"(Duration:\s*([\d\.]+)\s*seconds)");
static const std::regex RE_NUNIT_FAILED_TEST(R"(\d+\)\s*(.+))");
static const std::regex RE_NUNIT_TEST_SOURCE(R"(Source:\s*(.+):line\s*(\d+))");
static const std::regex RE_NUNIT_TEST_ASSERTION(R"(Expected:\s*(.+)\s*But was:\s*(.+))");

// xUnit patterns
static const std::regex RE_XUNIT_HEADER(R"(xUnit\.net VSTest Adapter\s+v([\d\.]+))");
static const std::regex RE_XUNIT_TEST_START(R"(Starting:\s*(.+))");
static const std::regex RE_XUNIT_TEST_FINISH(R"(Finished:\s*(.+))");
static const std::regex RE_XUNIT_TEST_PASS(R"(\s*(.+)\s*\[PASS\])");
static const std::regex RE_XUNIT_TEST_FAIL(R"(\s*(.+)\s*\[FAIL\])");
static const std::regex RE_XUNIT_TEST_SKIP(R"(\s*(.+)\s*\[SKIP\])");
static const std::regex RE_XUNIT_ASSERTION_FAILURE(R"(Assert\.(\w+)\(\)\s*Failure)");
static const std::regex RE_XUNIT_STACK_TRACE(R"(at\s+(.+)\s+in\s+(.+):line\s+(\d+))");
static const std::regex RE_XUNIT_TOTAL_SUMMARY(R"(Total tests:\s*(\d+))");
static const std::regex RE_XUNIT_PASSED_SUMMARY(R"(Passed:\s*(\d+))");
static const std::regex RE_XUNIT_FAILED_SUMMARY(R"(Failed:\s*(\d+))");
static const std::regex RE_XUNIT_SKIPPED_SUMMARY(R"(Skipped:\s*(\d+))");
static const std::regex RE_XUNIT_TIME_SUMMARY(R"(Time:\s*([\d\.]+)s)");
} // anonymous namespace

bool NUnitXUnitTextParser::canParse(const std::string &content) const {
	// Look for specific NUnit/xUnit markers
	bool has_nunit_marker = content.find("NUnit") != std::string::npos;
	bool has_xunit_marker = content.find("xUnit.net") != std::string::npos;
	bool has_test_count = content.find("Test Count:") != std::string::npos;
	bool has_overall_result = content.find("Overall result:") != std::string::npos;

	// Require at least one definitive marker (NUnit/xUnit name or summary format)
	// [PASS]/[FAIL]/[SKIP] alone are too generic
	if (has_nunit_marker || has_xunit_marker || has_test_count || has_overall_result) {
		return true;
	}

	// Only accept [PASS]/[FAIL]/[SKIP] if we also see dotnet-style test output patterns
	bool has_pass_fail = content.find("[PASS]") != std::string::npos || content.find("[FAIL]") != std::string::npos ||
	                     content.find("[SKIP]") != std::string::npos;
	bool has_dotnet_pattern = content.find("Microsoft.") != std::string::npos ||
	                          content.find(".Tests") != std::string::npos ||
	                          content.find("dotnet test") != std::string::npos;

	return has_pass_fail && has_dotnet_pattern;
}

// Forward declaration for implementation
static void parseNUnitXUnitImpl(const std::string &content, std::vector<ValidationEvent> &events);

std::vector<ValidationEvent> NUnitXUnitTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	parseNUnitXUnitImpl(content, events);
	return events;
}

static void parseNUnitXUnitImpl(const std::string &content, std::vector<ValidationEvent> &events) {
	events.reserve(content.size() / 100); // Estimate: ~1 event per 100 chars
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	std::string current_test_suite;
	std::string current_framework = "unknown";
	std::vector<std::string> failure_details;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Detect NUnit vs xUnit framework
		if (std::regex_search(line, match, RE_NUNIT_HEADER)) {
			current_framework = "nunit";

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "NUnit version " + match[1].str();
			event.tool_name = "nunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		} else if (std::regex_search(line, match, RE_XUNIT_HEADER)) {
			current_framework = "xunit";

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "xUnit.net VSTest Adapter version " + match[1].str();
			event.tool_name = "xunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// NUnit Test Summary
		else if (std::regex_search(line, match, RE_NUNIT_SUMMARY)) {
			int total_tests = SafeParsing::SafeStoi(match[1].str());
			int passed = SafeParsing::SafeStoi(match[2].str());
			int failed = SafeParsing::SafeStoi(match[3].str());
			int skipped = SafeParsing::SafeStoi(match[6].str());
			// Note: warnings (match[4]) and inconclusive (match[5]) are parsed but typically not critical for summaries

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = failed > 0 ? "error" : "info";
			event.status = failed > 0 ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.message = "Test summary: " + std::to_string(total_tests) + " total, " + std::to_string(passed) +
			                " passed, " + std::to_string(failed) + " failed, " + std::to_string(skipped) + " skipped";
			event.tool_name = "nunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// NUnit Overall Result
		else if (std::regex_search(line, match, RE_NUNIT_OVERALL_RESULT)) {
			std::string result = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = (result == "Failed") ? "error" : "info";
			event.status = (result == "Failed") ? ValidationEventStatus::FAIL : ValidationEventStatus::PASS;
			event.message = "Overall test result: " + result;
			event.tool_name = "nunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// NUnit Duration
		else if (std::regex_search(line, match, RE_NUNIT_DURATION)) {
			double duration_seconds = std::stod(match[1].str());
			int64_t duration_ms = static_cast<int64_t>(duration_seconds * 1000);

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::PERFORMANCE_METRIC;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Test execution time: " + match[1].str() + " seconds";
			event.execution_time = duration_ms;
			event.tool_name = "nunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// xUnit Test Suite Start
		else if (std::regex_search(line, match, RE_XUNIT_TEST_START)) {
			current_test_suite = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Starting test suite: " + current_test_suite;
			event.tool_name = "xunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// xUnit Test Suite Finish
		else if (std::regex_search(line, match, RE_XUNIT_TEST_FINISH)) {
			std::string suite_name = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::DEBUG_INFO;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Finished test suite: " + suite_name;
			event.tool_name = "xunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// xUnit Test Pass
		else if (std::regex_search(line, match, RE_XUNIT_TEST_PASS)) {
			std::string test_name = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "info";
			event.status = ValidationEventStatus::PASS;
			event.message = "Test passed: " + test_name;
			event.test_name = test_name;
			event.tool_name = "xunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// xUnit Test Fail
		else if (std::regex_search(line, match, RE_XUNIT_TEST_FAIL)) {
			std::string test_name = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "error";
			event.status = ValidationEventStatus::FAIL;
			event.message = "Test failed: " + test_name;
			event.test_name = test_name;
			event.tool_name = "xunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// xUnit Test Skip
		else if (std::regex_search(line, match, RE_XUNIT_TEST_SKIP)) {
			std::string test_name = match[1].str();

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::TEST_RESULT;
			event.severity = "warning";
			event.status = ValidationEventStatus::SKIP;
			event.message = "Test skipped: " + test_name;
			event.test_name = test_name;
			event.tool_name = "xunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// xUnit Test Summary
		else if (std::regex_search(line, match, RE_XUNIT_TOTAL_SUMMARY)) {
			int total_tests = SafeParsing::SafeStoi(match[1].str());

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::SUMMARY;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Total tests: " + std::to_string(total_tests);
			event.tool_name = "xunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.execution_time = 0;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// xUnit Time Summary
		else if (std::regex_search(line, match, RE_XUNIT_TIME_SUMMARY)) {
			double duration_seconds = std::stod(match[1].str());
			int64_t duration_ms = static_cast<int64_t>(duration_seconds * 1000);

			ValidationEvent event;
			event.event_id = event_id++;
			event.event_type = ValidationEventType::PERFORMANCE_METRIC;
			event.severity = "info";
			event.status = ValidationEventStatus::INFO;
			event.message = "Test execution time: " + match[1].str() + " seconds";
			event.execution_time = duration_ms;
			event.tool_name = "xunit";
			event.category = "nunit_xunit_text";
			event.log_content = line;
			event.log_line_start = current_line_num;
			event.log_line_end = current_line_num;

			events.push_back(event);
		}
		// Handle file path and line number information
		else if (std::regex_search(line, match, RE_NUNIT_TEST_SOURCE)) {
			std::string file_path = match[1].str();
			int line_number = SafeParsing::SafeStoi(match[2].str());

			// Update the most recent test event with file/line info
			if (!events.empty()) {
				ValidationEvent &last_event = events.back();
				if (last_event.category == "nunit_xunit_text" && last_event.ref_file.empty()) {
					last_event.ref_file = file_path;
					last_event.ref_line = line_number;
				}
			}
		}
		// Handle xUnit stack traces
		else if (std::regex_search(line, match, RE_XUNIT_STACK_TRACE)) {
			std::string file_path = match[2].str();
			int line_number = SafeParsing::SafeStoi(match[3].str());

			// Update the most recent test event with file/line info
			if (!events.empty()) {
				ValidationEvent &last_event = events.back();
				if (last_event.category == "nunit_xunit_text" && last_event.ref_file.empty()) {
					last_event.ref_file = file_path;
					last_event.ref_line = line_number;
				}
			}
		}
	}
}

} // namespace duckdb
