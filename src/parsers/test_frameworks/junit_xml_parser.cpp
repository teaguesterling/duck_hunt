#include "junit_xml_parser.hpp"
#include "yyjson.hpp"
#include <sstream>

namespace duckdb {

using namespace duckdb_yyjson;

// Forward declarations for helper functions
static void ParseTestSuiteFromJson(yyjson_val *suite, std::vector<ValidationEvent> &events, int64_t &event_id);
static void ParseTestCaseFromJson(yyjson_val *testcase, const std::string &suite_name,
                                  std::vector<ValidationEvent> &events, int64_t &event_id);

bool JUnitXmlParser::canParse(const std::string &content) const {
	if (!LooksLikeXml(content)) {
		return false;
	}

	// Check for JUnit XML markers
	return HasRootElement(content, "testsuite") || HasRootElement(content, "testsuites");
}

std::vector<ValidationEvent> JUnitXmlParser::parseJsonContent(const std::string &json_content) const {
	std::vector<ValidationEvent> events;
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

	// Handle both <testsuites> and single <testsuite> root
	yyjson_val *testsuites = yyjson_obj_get(root, "testsuites");
	yyjson_val *testsuite = yyjson_obj_get(root, "testsuite");

	if (testsuites) {
		// Multiple testsuites
		yyjson_val *suites = yyjson_obj_get(testsuites, "testsuite");
		if (suites) {
			if (yyjson_is_arr(suites)) {
				size_t idx, max;
				yyjson_val *suite;
				yyjson_arr_foreach(suites, idx, max, suite) {
					ParseTestSuiteFromJson(suite, events, event_id);
				}
			} else if (yyjson_is_obj(suites)) {
				ParseTestSuiteFromJson(suites, events, event_id);
			}
		}
	} else if (testsuite) {
		// Single testsuite at root
		if (yyjson_is_arr(testsuite)) {
			size_t idx, max;
			yyjson_val *suite;
			yyjson_arr_foreach(testsuite, idx, max, suite) {
				ParseTestSuiteFromJson(suite, events, event_id);
			}
		} else if (yyjson_is_obj(testsuite)) {
			ParseTestSuiteFromJson(testsuite, events, event_id);
		}
	}

	yyjson_doc_free(doc);
	return events;
}

static void ParseTestSuiteFromJson(yyjson_val *suite, std::vector<ValidationEvent> &events, int64_t &event_id) {
	if (!suite || !yyjson_is_obj(suite)) {
		return;
	}

	// Get testsuite attributes
	std::string suite_name;
	yyjson_val *name_val = yyjson_obj_get(suite, "@name");
	if (name_val && yyjson_is_str(name_val)) {
		suite_name = yyjson_get_str(name_val);
	}

	// Get testcases
	yyjson_val *testcases = yyjson_obj_get(suite, "testcase");
	if (!testcases) {
		return;
	}

	// Handle both single testcase and array of testcases
	if (yyjson_is_arr(testcases)) {
		size_t idx, max;
		yyjson_val *testcase;
		yyjson_arr_foreach(testcases, idx, max, testcase) {
			ParseTestCaseFromJson(testcase, suite_name, events, event_id);
		}
	} else if (yyjson_is_obj(testcases)) {
		ParseTestCaseFromJson(testcases, suite_name, events, event_id);
	}
}

static void ParseTestCaseFromJson(yyjson_val *testcase, const std::string &suite_name,
                                  std::vector<ValidationEvent> &events, int64_t &event_id) {
	if (!testcase || !yyjson_is_obj(testcase)) {
		return;
	}

	ValidationEvent event;
	event.event_id = event_id++;
	event.tool_name = "junit";
	event.event_type = ValidationEventType::TEST_RESULT;

	// Extract test name
	yyjson_val *name_val = yyjson_obj_get(testcase, "@name");
	if (name_val && yyjson_is_str(name_val)) {
		event.test_name = yyjson_get_str(name_val);
	}

	// Extract classname as file_path
	yyjson_val *classname_val = yyjson_obj_get(testcase, "@classname");
	if (classname_val && yyjson_is_str(classname_val)) {
		event.ref_file = yyjson_get_str(classname_val);
	}

	// Extract time
	yyjson_val *time_val = yyjson_obj_get(testcase, "@time");
	if (time_val && yyjson_is_str(time_val)) {
		try {
			event.execution_time = std::stod(yyjson_get_str(time_val));
		} catch (...) {
			event.execution_time = 0.0;
		}
	}

	// Check for failure
	yyjson_val *failure = yyjson_obj_get(testcase, "failure");
	if (failure) {
		event.status = ValidationEventStatus::FAIL;
		event.severity = "error";
		event.category = "test_failure";

		if (yyjson_is_obj(failure)) {
			yyjson_val *msg = yyjson_obj_get(failure, "@message");
			if (msg && yyjson_is_str(msg)) {
				event.message = yyjson_get_str(msg);
			}
			yyjson_val *text = yyjson_obj_get(failure, "#text");
			if (text && yyjson_is_str(text)) {
				event.log_content = yyjson_get_str(text);
			}
		} else if (yyjson_is_str(failure)) {
			event.message = yyjson_get_str(failure);
		}
	}
	// Check for error
	else {
		yyjson_val *error = yyjson_obj_get(testcase, "error");
		if (error) {
			event.status = ValidationEventStatus::ERROR;
			event.severity = "error";
			event.category = "test_error";

			if (yyjson_is_obj(error)) {
				yyjson_val *msg = yyjson_obj_get(error, "@message");
				if (msg && yyjson_is_str(msg)) {
					event.message = yyjson_get_str(msg);
				}
				yyjson_val *type = yyjson_obj_get(error, "@type");
				if (type && yyjson_is_str(type)) {
					event.error_code = yyjson_get_str(type);
				}
				yyjson_val *text = yyjson_obj_get(error, "#text");
				if (text && yyjson_is_str(text)) {
					event.log_content = yyjson_get_str(text);
				}
			} else if (yyjson_is_str(error)) {
				event.message = yyjson_get_str(error);
			}
		}
		// Check for skipped
		else {
			yyjson_val *skipped = yyjson_obj_get(testcase, "skipped");
			if (skipped) {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "info";
				event.category = "test_skipped";

				if (yyjson_is_obj(skipped)) {
					yyjson_val *msg = yyjson_obj_get(skipped, "@message");
					if (msg && yyjson_is_str(msg)) {
						event.message = yyjson_get_str(msg);
					}
				} else if (yyjson_is_str(skipped)) {
					event.message = yyjson_get_str(skipped);
				}
			}
			// Test passed (no failure, error, or skipped)
			else {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
				event.category = "test_pass";
				event.message = "Test passed";
			}
		}
	}

	// Build full test name with suite
	if (!suite_name.empty() && !event.test_name.empty()) {
		event.function_name = suite_name + "::" + event.test_name;
	} else {
		event.function_name = event.test_name;
	}

	event.structured_data = "junit_xml";

	events.push_back(event);
}

} // namespace duckdb
