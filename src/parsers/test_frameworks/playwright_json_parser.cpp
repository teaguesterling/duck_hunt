#include "playwright_json_parser.hpp"
#include "include/validation_event_types.hpp"

namespace duckdb {

using namespace duckdb_yyjson;

bool PlaywrightJSONParser::canParse(const std::string &content) const {
	// Quick checks before full JSON parsing
	if (content.find("\"suites\"") == std::string::npos) {
		return false;
	}
	// Must have stats or config (Playwright-specific)
	if (content.find("\"stats\"") == std::string::npos && content.find("\"config\"") == std::string::npos) {
		return false;
	}

	return isValidPlaywrightJSON(content);
}

bool PlaywrightJSONParser::isValidPlaywrightJSON(const std::string &content) const {
	yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
	if (!doc) {
		return false;
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	bool is_valid = false;

	if (yyjson_is_obj(root)) {
		yyjson_val *suites = yyjson_obj_get(root, "suites");
		yyjson_val *stats = yyjson_obj_get(root, "stats");

		// Must have suites array
		if (suites && yyjson_is_arr(suites)) {
			// Playwright JSON has stats with expected/unexpected/skipped/flaky
			if (stats && yyjson_is_obj(stats)) {
				yyjson_val *expected = yyjson_obj_get(stats, "expected");
				yyjson_val *unexpected = yyjson_obj_get(stats, "unexpected");
				// These are Playwright-specific stat fields
				is_valid = (expected != nullptr) || (unexpected != nullptr);
			}
		}
	}

	yyjson_doc_free(doc);
	return is_valid;
}

void PlaywrightJSONParser::parseSpec(yyjson_val *spec, std::vector<ValidationEvent> &events, int64_t &event_id,
                                     const std::string &suite_title) const {
	if (!yyjson_is_obj(spec))
		return;

	// Get spec info
	yyjson_val *title_val = yyjson_obj_get(spec, "title");
	yyjson_val *file_val = yyjson_obj_get(spec, "file");
	yyjson_val *line_val = yyjson_obj_get(spec, "line");
	yyjson_val *column_val = yyjson_obj_get(spec, "column");
	yyjson_val *ok_val = yyjson_obj_get(spec, "ok");

	std::string spec_title = title_val && yyjson_is_str(title_val) ? yyjson_get_str(title_val) : "";
	std::string file = file_val && yyjson_is_str(file_val) ? yyjson_get_str(file_val) : "";
	int line = line_val && yyjson_is_int(line_val) ? yyjson_get_int(line_val) : -1;
	int column = column_val && yyjson_is_int(column_val) ? yyjson_get_int(column_val) : -1;

	// Build full test name
	std::string full_test_name = suite_title.empty() ? spec_title : suite_title + " › " + spec_title;

	// Get tests array
	yyjson_val *tests = yyjson_obj_get(spec, "tests");
	if (!tests || !yyjson_is_arr(tests))
		return;

	size_t test_idx, test_max;
	yyjson_val *test;
	yyjson_arr_foreach(tests, test_idx, test_max, test) {
		if (!yyjson_is_obj(test))
			continue;

		// Get project info
		yyjson_val *project_name = yyjson_obj_get(test, "projectName");
		std::string browser = project_name && yyjson_is_str(project_name) ? yyjson_get_str(project_name) : "";

		// Get results array
		yyjson_val *results = yyjson_obj_get(test, "results");
		if (!results || !yyjson_is_arr(results))
			continue;

		size_t result_idx, result_max;
		yyjson_val *result;
		yyjson_arr_foreach(results, result_idx, result_max, result) {
			if (!yyjson_is_obj(result))
				continue;

			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "playwright";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.ref_file = file;
			event.ref_line = line;
			event.ref_column = column;
			event.test_name = full_test_name;
			event.category = "playwright_json";

			// Get status
			yyjson_val *status_val = yyjson_obj_get(result, "status");
			std::string status = status_val && yyjson_is_str(status_val) ? yyjson_get_str(status_val) : "";

			// Get duration
			yyjson_val *duration_val = yyjson_obj_get(result, "duration");
			event.execution_time = duration_val && yyjson_is_num(duration_val) ? yyjson_get_real(duration_val) : 0.0;

			// Map status to event status
			if (status == "passed") {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
				event.message = "Test passed: " + full_test_name;
			} else if (status == "failed" || status == "timedOut") {
				event.status = ValidationEventStatus::FAIL;
				event.severity = "error";
				event.message = "Test failed: " + full_test_name;

				// Get error details
				yyjson_val *error = yyjson_obj_get(result, "error");
				if (error && yyjson_is_obj(error)) {
					yyjson_val *error_msg = yyjson_obj_get(error, "message");
					if (error_msg && yyjson_is_str(error_msg)) {
						// Clean ANSI escape codes from message
						std::string msg = yyjson_get_str(error_msg);
						// Simple ANSI removal
						size_t pos;
						while ((pos = msg.find("\x1b[")) != std::string::npos) {
							size_t end = msg.find('m', pos);
							if (end != std::string::npos) {
								msg.erase(pos, end - pos + 1);
							} else {
								break;
							}
						}
						event.message = msg;
					}

					// Get error location if available
					yyjson_val *location = yyjson_obj_get(error, "location");
					if (location && yyjson_is_obj(location)) {
						yyjson_val *loc_file = yyjson_obj_get(location, "file");
						yyjson_val *loc_line = yyjson_obj_get(location, "line");
						yyjson_val *loc_col = yyjson_obj_get(location, "column");

						if (loc_file && yyjson_is_str(loc_file)) {
							event.ref_file = yyjson_get_str(loc_file);
						}
						if (loc_line && yyjson_is_int(loc_line)) {
							event.ref_line = yyjson_get_int(loc_line);
						}
						if (loc_col && yyjson_is_int(loc_col)) {
							event.ref_column = yyjson_get_int(loc_col);
						}
					}
				}
			} else if (status == "skipped") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "warning";
				event.message = "Test skipped: " + full_test_name;
			} else if (status == "interrupted") {
				event.status = ValidationEventStatus::ERROR;
				event.severity = "error";
				event.message = "Test interrupted: " + full_test_name;
			} else {
				event.status = ValidationEventStatus::INFO;
				event.severity = "info";
				event.message = "Test " + status + ": " + full_test_name;
			}

			// Add browser to structured data
			if (!browser.empty()) {
				event.structured_data = "{\"browser\": \"" + browser + "\"}";
			}

			events.push_back(event);
		}
	}
}

void PlaywrightJSONParser::parseSuite(yyjson_val *suite, std::vector<ValidationEvent> &events, int64_t &event_id,
                                      const std::string &parent_title) const {
	if (!yyjson_is_obj(suite))
		return;

	// Get suite title
	yyjson_val *title_val = yyjson_obj_get(suite, "title");
	std::string title = title_val && yyjson_is_str(title_val) ? yyjson_get_str(title_val) : "";

	// Build full suite title
	std::string full_title = parent_title.empty() ? title : parent_title + " › " + title;

	// Parse specs in this suite
	yyjson_val *specs = yyjson_obj_get(suite, "specs");
	if (specs && yyjson_is_arr(specs)) {
		size_t idx, max;
		yyjson_val *spec;
		yyjson_arr_foreach(specs, idx, max, spec) {
			parseSpec(spec, events, event_id, full_title);
		}
	}

	// Recursively parse nested suites
	yyjson_val *suites = yyjson_obj_get(suite, "suites");
	if (suites && yyjson_is_arr(suites)) {
		size_t idx, max;
		yyjson_val *nested_suite;
		yyjson_arr_foreach(suites, idx, max, nested_suite) {
			parseSuite(nested_suite, events, event_id, full_title);
		}
	}
}

std::vector<ValidationEvent> PlaywrightJSONParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;

	yyjson_doc *doc = yyjson_read(content.c_str(), content.length(), 0);
	if (!doc) {
		throw IOException("Failed to parse Playwright JSON");
	}

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!yyjson_is_obj(root)) {
		yyjson_doc_free(doc);
		throw IOException("Invalid Playwright JSON: root is not an object");
	}

	int64_t event_id = 1;

	// Parse suites recursively
	yyjson_val *suites = yyjson_obj_get(root, "suites");
	if (suites && yyjson_is_arr(suites)) {
		size_t idx, max;
		yyjson_val *suite;
		yyjson_arr_foreach(suites, idx, max, suite) {
			parseSuite(suite, events, event_id, "");
		}
	}

	// Parse stats for summary
	yyjson_val *stats = yyjson_obj_get(root, "stats");
	if (stats && yyjson_is_obj(stats)) {
		ValidationEvent summary;
		summary.event_id = event_id++;
		summary.event_type = ValidationEventType::SUMMARY;
		summary.tool_name = "playwright";
		summary.category = "playwright_json";
		summary.ref_line = -1;
		summary.ref_column = -1;

		int expected = 0, unexpected = 0, skipped = 0, flaky = 0;
		double duration = 0.0;

		yyjson_val *v;
		if ((v = yyjson_obj_get(stats, "expected")) && yyjson_is_int(v))
			expected = yyjson_get_int(v);
		if ((v = yyjson_obj_get(stats, "unexpected")) && yyjson_is_int(v))
			unexpected = yyjson_get_int(v);
		if ((v = yyjson_obj_get(stats, "skipped")) && yyjson_is_int(v))
			skipped = yyjson_get_int(v);
		if ((v = yyjson_obj_get(stats, "flaky")) && yyjson_is_int(v))
			flaky = yyjson_get_int(v);
		if ((v = yyjson_obj_get(stats, "duration")) && yyjson_is_num(v))
			duration = yyjson_get_real(v);

		summary.execution_time = duration;
		summary.status = (unexpected > 0) ? ValidationEventStatus::FAIL : ValidationEventStatus::INFO;
		summary.severity = (unexpected > 0) ? "error" : "info";

		// Build message
		summary.message = std::to_string(expected) + " passed";
		if (unexpected > 0)
			summary.message += ", " + std::to_string(unexpected) + " failed";
		if (skipped > 0)
			summary.message += ", " + std::to_string(skipped) + " skipped";
		if (flaky > 0)
			summary.message += ", " + std::to_string(flaky) + " flaky";

		summary.structured_data = "{\"passed\":" + std::to_string(expected) +
		                          ",\"failed\":" + std::to_string(unexpected) +
		                          ",\"skipped\":" + std::to_string(skipped) + ",\"flaky\":" + std::to_string(flaky) +
		                          ",\"duration\":" + std::to_string(duration) + "}";

		events.push_back(summary);
	}

	yyjson_doc_free(doc);
	return events;
}

} // namespace duckdb
