#include "unity_test_xml_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include "yyjson.hpp"
#include <sstream>

namespace duckdb {

using namespace duckdb_yyjson;

// Forward declarations for helper functions
static void ParseTestSuiteFromJson(yyjson_val *suite, std::vector<ValidationEvent> &events, int64_t &event_id);
static void ParseTestCaseFromJson(yyjson_val *testcase, std::vector<ValidationEvent> &events, int64_t &event_id);
static std::string GetStringAttr(yyjson_val *obj, const char *attr);
static std::string GetTextContent(yyjson_val *element);

bool UnityTestXmlParser::canParse(const std::string &content) const {
	if (!LooksLikeXml(content)) {
		return false;
	}

	// Check for Unity/NUnit 3 test-run root element
	if (!HasRootElement(content, "test-run")) {
		return false;
	}

	// Additional check: look for NUnit 3 specific attributes
	// Unity Test Runner uses NUnit 3 format with specific markers
	return content.find("testcasecount=") != std::string::npos || content.find("engine-version=") != std::string::npos;
}

std::vector<ValidationEvent> UnityTestXmlParser::parseJsonContent(const std::string &json_content) const {
	std::vector<ValidationEvent> events;
	events.reserve(64); // Pre-allocate for typical test runs
	int64_t event_id = 1;

	// Parse JSON using yyjson
	yyjson_doc *doc = yyjson_read(json_content.c_str(), json_content.length(), 0);
	if (!doc) {
		return events;
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!root) {
		yyjson_doc_free(doc);
		return events;
	}

	// Get test-run element (webbed converts to "test-run" key)
	yyjson_val *test_run = yyjson_obj_get(root, "test-run");
	if (!test_run || !yyjson_is_obj(test_run)) {
		yyjson_doc_free(doc);
		return events;
	}

	// Process nested test-suites
	yyjson_val *test_suite = yyjson_obj_get(test_run, "test-suite");
	if (test_suite) {
		if (yyjson_is_arr(test_suite)) {
			size_t idx, max;
			yyjson_val *suite;
			yyjson_arr_foreach(test_suite, idx, max, suite) {
				ParseTestSuiteFromJson(suite, events, event_id);
			}
		} else if (yyjson_is_obj(test_suite)) {
			ParseTestSuiteFromJson(test_suite, events, event_id);
		}
	}

	yyjson_doc_free(doc);
	return events;
}

static std::string GetStringAttr(yyjson_val *obj, const char *attr) {
	if (!obj || !yyjson_is_obj(obj)) {
		return "";
	}
	yyjson_val *val = yyjson_obj_get(obj, attr);
	if (val && yyjson_is_str(val)) {
		return yyjson_get_str(val);
	}
	return "";
}

static std::string GetTextContent(yyjson_val *element) {
	if (!element) {
		return "";
	}

	// Direct string content
	if (yyjson_is_str(element)) {
		return yyjson_get_str(element);
	}

	// Object with #text property (common webbed pattern)
	if (yyjson_is_obj(element)) {
		yyjson_val *text = yyjson_obj_get(element, "#text");
		if (text && yyjson_is_str(text)) {
			return yyjson_get_str(text);
		}
	}

	return "";
}

static void ParseTestSuiteFromJson(yyjson_val *suite, std::vector<ValidationEvent> &events, int64_t &event_id) {
	if (!suite || !yyjson_is_obj(suite)) {
		return;
	}

	// Recursively process nested test-suites
	yyjson_val *nested_suites = yyjson_obj_get(suite, "test-suite");
	if (nested_suites) {
		if (yyjson_is_arr(nested_suites)) {
			size_t idx, max;
			yyjson_val *nested;
			yyjson_arr_foreach(nested_suites, idx, max, nested) {
				ParseTestSuiteFromJson(nested, events, event_id);
			}
		} else if (yyjson_is_obj(nested_suites)) {
			ParseTestSuiteFromJson(nested_suites, events, event_id);
		}
	}

	// Process test-cases in this suite
	yyjson_val *test_cases = yyjson_obj_get(suite, "test-case");
	if (test_cases) {
		if (yyjson_is_arr(test_cases)) {
			size_t idx, max;
			yyjson_val *testcase;
			yyjson_arr_foreach(test_cases, idx, max, testcase) {
				ParseTestCaseFromJson(testcase, events, event_id);
			}
		} else if (yyjson_is_obj(test_cases)) {
			ParseTestCaseFromJson(test_cases, events, event_id);
		}
	}
}

static void ParseTestCaseFromJson(yyjson_val *testcase, std::vector<ValidationEvent> &events, int64_t &event_id) {
	if (!testcase || !yyjson_is_obj(testcase)) {
		return;
	}

	ValidationEvent event;
	event.event_id = event_id++;
	event.tool_name = "unity_test";
	event.event_type = ValidationEventType::TEST_RESULT;

	// Extract test identifiers
	std::string name = GetStringAttr(testcase, "@name");
	std::string fullname = GetStringAttr(testcase, "@fullname");
	std::string methodname = GetStringAttr(testcase, "@methodname");
	std::string classname = GetStringAttr(testcase, "@classname");
	std::string result = GetStringAttr(testcase, "@result");

	// Use fullname as test_name if available, otherwise name
	event.test_name = !fullname.empty() ? fullname : name;
	event.function_name = !methodname.empty() ? methodname : name;
	event.ref_file = classname;

	// Extract duration
	std::string duration = GetStringAttr(testcase, "@duration");
	if (!duration.empty()) {
		try {
			event.execution_time = SafeParsing::SafeStod(duration);
		} catch (...) {
			event.execution_time = 0.0;
		}
	}

	// Determine status from result attribute
	if (result == "Passed") {
		event.status = ValidationEventStatus::PASS;
		event.severity = "info";
		event.category = "test_pass";
		event.message = "Test passed";
	} else if (result == "Failed") {
		event.status = ValidationEventStatus::FAIL;
		event.severity = "error";
		event.category = "test_failure";

		// Extract failure details
		yyjson_val *failure = yyjson_obj_get(testcase, "failure");
		if (failure && yyjson_is_obj(failure)) {
			yyjson_val *message = yyjson_obj_get(failure, "message");
			event.message = GetTextContent(message);

			yyjson_val *stack_trace = yyjson_obj_get(failure, "stack-trace");
			std::string trace = GetTextContent(stack_trace);
			if (!trace.empty()) {
				event.log_content = trace;
			}
		}
	} else if (result == "Skipped" || result == "Ignored") {
		event.status = ValidationEventStatus::SKIP;
		event.severity = "info";
		event.category = "test_skipped";

		// Extract skip reason
		yyjson_val *reason = yyjson_obj_get(testcase, "reason");
		if (reason && yyjson_is_obj(reason)) {
			yyjson_val *message = yyjson_obj_get(reason, "message");
			event.message = GetTextContent(message);
		}
		if (event.message.empty()) {
			event.message = "Test skipped";
		}
	} else if (result == "Inconclusive") {
		event.status = ValidationEventStatus::WARNING;
		event.severity = "warning";
		event.category = "test_inconclusive";
		event.message = "Test inconclusive";
	} else {
		// Unknown result - treat as warning
		event.status = ValidationEventStatus::WARNING;
		event.severity = "warning";
		event.category = "test_unknown";
		event.message = "Unknown test result: " + result;
	}

	// Extract test output if available
	yyjson_val *output = yyjson_obj_get(testcase, "output");
	std::string output_text = GetTextContent(output);
	if (!output_text.empty() && event.log_content.empty()) {
		event.log_content = output_text;
	} else if (!output_text.empty()) {
		// Append output to existing log_content (stack trace)
		event.log_content += "\n\n--- Test Output ---\n" + output_text;
	}

	event.structured_data = "unity_test_xml";

	events.push_back(std::move(event));
}

} // namespace duckdb
