#include "gotest_text_parser.hpp"
#include <sstream>
#include <unordered_map>

namespace duckdb {

bool GoTestTextParser::canParse(const std::string &content) const {
	// Look for Go test-specific patterns:
	// 1. "=== RUN" lines
	// 2. "--- PASS:" or "--- FAIL:" or "--- SKIP:" lines
	// 3. Package summary lines like "ok  pkg  0.001s" or "FAIL  pkg  0.001s"

	// Check for RUN marker
	if (content.find("=== RUN") != std::string::npos) {
		// Verify we also have result markers
		if (content.find("--- PASS:") != std::string::npos || content.find("--- FAIL:") != std::string::npos ||
		    content.find("--- SKIP:") != std::string::npos) {
			return true;
		}
	}

	// Check for package summary without individual tests
	std::regex pkg_summary(R"((ok|FAIL)\s+\S+\s+[\d.]+s)");
	if (std::regex_search(content, pkg_summary)) {
		return true;
	}

	return false;
}

std::vector<ValidationEvent> GoTestTextParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int32_t current_line_num = 0;

	// Pattern for test result: "--- PASS: TestName (0.00s)"
	std::regex result_pattern(R"(^---\s+(PASS|FAIL|SKIP):\s+(\S+)\s+\(([\d.]+)s\))");

	// Pattern for error location: "    file_test.go:15: message"
	std::regex error_location_pattern(R"(^\s+([^:]+\.go):(\d+):\s*(.+)$)");

	// Pattern for package summary: "FAIL  pkg  0.001s" or "ok  pkg  0.001s"
	std::regex pkg_pattern(R"(^(ok|FAIL)\s+(\S+)\s+([\d.]+)s)");

	int pass_count = 0;
	int fail_count = 0;
	int skip_count = 0;

	// Track current test for associating error messages
	std::string current_test;
	std::vector<std::string> current_errors;
	int32_t test_start_line = 0;

	// Store test info for associating errors
	struct TestInfo {
		std::string name;
		std::string status;
		double duration;
		std::vector<std::tuple<std::string, int32_t, std::string>> errors; // file, line, message
		int32_t start_line;
		int32_t end_line;
	};
	std::vector<TestInfo> tests;
	TestInfo *current_test_info = nullptr;

	while (std::getline(stream, line)) {
		current_line_num++;
		std::smatch match;

		// Check for RUN marker
		if (line.find("=== RUN") != std::string::npos) {
			size_t pos = line.find("=== RUN");
			std::string test_name = line.substr(pos + 8);
			// Trim whitespace
			while (!test_name.empty() && (test_name.front() == ' ' || test_name.front() == '\t')) {
				test_name.erase(0, 1);
			}
			while (!test_name.empty() && (test_name.back() == ' ' || test_name.back() == '\t')) {
				test_name.pop_back();
			}

			tests.push_back({test_name, "", 0.0, {}, current_line_num, 0});
			current_test_info = &tests.back();
			continue;
		}

		// Check for error location in current test
		if (current_test_info && std::regex_match(line, match, error_location_pattern)) {
			std::string file = match[1].str();
			int32_t err_line = 0;
			try {
				err_line = std::stoi(match[2].str());
			} catch (...) {
			}
			std::string message = match[3].str();
			current_test_info->errors.push_back({file, err_line, message});
			continue;
		}

		// Check for test result
		if (std::regex_search(line, match, result_pattern)) {
			std::string status = match[1].str();
			std::string test_name = match[2].str();
			double duration = 0.0;
			try {
				duration = std::stod(match[3].str());
			} catch (...) {
			}

			// Find or create test info
			TestInfo *test_info = nullptr;
			for (auto &t : tests) {
				if (t.name == test_name) {
					test_info = &t;
					break;
				}
			}
			if (!test_info) {
				tests.push_back({test_name, status, duration, {}, current_line_num, current_line_num});
				test_info = &tests.back();
			} else {
				test_info->status = status;
				test_info->duration = duration;
				test_info->end_line = current_line_num;
			}

			current_test_info = nullptr; // Reset for next test
			continue;
		}

		// Check for package summary
		if (std::regex_match(line, match, pkg_pattern)) {
			std::string status = match[1].str();
			std::string package = match[2].str();
			double duration = 0.0;
			try {
				duration = std::stod(match[3].str());
			} catch (...) {
			}

			// If no individual tests were found, create a package-level event
			if (tests.empty()) {
				ValidationEvent event;
				event.event_id = event_id++;
				event.event_type = ValidationEventType::TEST_RESULT;
				event.tool_name = "go_test";
				event.test_name = package;
				event.execution_time = duration;
				event.category = "test";
				event.log_line_start = current_line_num;
				event.log_line_end = current_line_num;

				if (status == "ok") {
					event.status = ValidationEventStatus::PASS;
					event.severity = "info";
					event.message = "Package tests passed";
					pass_count++;
				} else {
					event.status = ValidationEventStatus::FAIL;
					event.severity = "error";
					event.message = "Package tests failed";
					fail_count++;
				}

				events.push_back(event);
			}
		}
	}

	// Convert collected tests to events
	for (const auto &test : tests) {
		ValidationEvent event;
		event.event_id = event_id++;
		event.event_type = ValidationEventType::TEST_RESULT;
		event.tool_name = "go_test";
		event.test_name = test.name;
		event.execution_time = test.duration;
		event.category = "test";
		event.log_line_start = test.start_line;
		event.log_line_end = test.end_line > 0 ? test.end_line : test.start_line;

		if (test.status == "PASS") {
			event.status = ValidationEventStatus::PASS;
			event.severity = "info";
			event.message = "Test passed";
			pass_count++;
		} else if (test.status == "FAIL") {
			event.status = ValidationEventStatus::FAIL;
			event.severity = "error";
			fail_count++;

			// Build error message from collected errors
			if (!test.errors.empty()) {
				auto &first_error = test.errors[0];
				event.ref_file = std::get<0>(first_error);
				event.ref_line = std::get<1>(first_error);
				event.message = std::get<2>(first_error);
			} else {
				event.message = "Test failed";
			}
		} else if (test.status == "SKIP") {
			event.status = ValidationEventStatus::SKIP;
			event.severity = "info";
			event.message = "Test skipped";
			skip_count++;
		} else {
			// Test started but no result found
			event.status = ValidationEventStatus::INFO;
			event.severity = "info";
			event.message = "Test incomplete";
		}

		events.push_back(event);
	}

	// Add summary event
	ValidationEvent summary;
	summary.event_id = event_id++;
	summary.event_type = ValidationEventType::SUMMARY;
	summary.tool_name = "go_test";
	summary.category = "test_summary";
	summary.ref_file = "";
	summary.ref_line = -1;
	summary.ref_column = -1;

	size_t total_tests = pass_count + fail_count + skip_count;
	if (fail_count > 0) {
		summary.status = ValidationEventStatus::FAIL;
		summary.severity = "error";
		summary.message = std::to_string(fail_count) + " of " + std::to_string(total_tests) + " test(s) failed";
	} else if (total_tests > 0) {
		summary.status = ValidationEventStatus::PASS;
		summary.severity = "info";
		summary.message = "All " + std::to_string(total_tests) + " test(s) passed";
	} else {
		summary.status = ValidationEventStatus::INFO;
		summary.severity = "info";
		summary.message = "No tests found";
	}

	summary.structured_data = "{\"passed\": " + std::to_string(pass_count) +
	                          ", \"failed\": " + std::to_string(fail_count) +
	                          ", \"skipped\": " + std::to_string(skip_count) + "}";
	events.push_back(summary);

	return events;
}

} // namespace duckdb
