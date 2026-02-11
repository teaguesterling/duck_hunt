#include "duckdb_test_parser.hpp"
#include <sstream>
#include <string>

namespace duckdb {

bool DuckDBTestParser::canParse(const std::string &content) const {
	// Check for DuckDB test output patterns
	return (content.find("unittest is a Catch") != std::string::npos ||
	        content.find("test cases:") != std::string::npos ||
	        (content.find("[") == 0 && content.find("]: test/") != std::string::npos) ||
	        content.find("Wrong result in query!") != std::string::npos);
}

std::vector<ValidationEvent> DuckDBTestParser::parse(const std::string &content) const {
	std::vector<ValidationEvent> events;
	ParseDuckDBTestOutput(content, events);
	return events;
}

// Static method for backward compatibility with legacy code
void DuckDBTestParser::ParseDuckDBTestOutput(const std::string &content, std::vector<ValidationEvent> &events) {
	std::istringstream stream(content);
	std::string line;
	int64_t event_id = 1;
	int line_num = 0;

	// DEBUG: Track parsing state
	bool debug = false; // Set to true to enable debug output

	// Track current test being processed
	std::string current_test_file;
	bool in_failure_section = false;
	std::string failure_message;
	std::string failure_query;
	int failure_line = -1;
	std::string mismatch_details;
	std::string expected_result;
	std::string actual_result;
	bool in_expected_section = false;
	bool in_actual_section = false;

	while (std::getline(stream, line)) {
		line_num++;
		if (debug) {
			fprintf(stderr, "DEBUG[%d]: '%s' (in_failure=%d)\n", line_num, line.substr(0, 50).c_str(),
			        in_failure_section);
		}

		// Parse test progress lines: [X/Y] (Z%): /path/to/test.test
		if (line.find("[") == 0 && line.find("]:") != std::string::npos) {
			size_t path_start = line.find("): ");
			if (path_start != std::string::npos) {
				current_test_file = line.substr(path_start + 3);
				// Trim any trailing whitespace/dots
				while (!current_test_file.empty() &&
				       (current_test_file.back() == '.' || current_test_file.back() == ' ')) {
					current_test_file.pop_back();
				}
			}
		}

		// Detect failure start - various DuckDB test failure messages
		else if (line.find("Wrong result in query!") != std::string::npos ||
		         line.find("Wrong row count in query!") != std::string::npos ||
		         line.find("Wrong column count in query!") != std::string::npos ||
		         line.find("Wrong result hash!") != std::string::npos ||
		         line.find("Query unexpectedly failed") != std::string::npos ||
		         line.find("Query unexpectedly succeeded!") != std::string::npos) {
			in_failure_section = true;
			failure_message = line;
			if (debug) {
				fprintf(stderr, "DEBUG: FAILURE DETECTED: '%s'\n", failure_message.c_str());
			}

			// Extract test file path and line number from failure message
			// Format: "Wrong result in query! (path/to/test.test:LINE)!"
			size_t paren_start = line.find("(");
			size_t paren_end = line.rfind(")");
			if (paren_start != std::string::npos && paren_end != std::string::npos && paren_end > paren_start) {
				std::string location = line.substr(paren_start + 1, paren_end - paren_start - 1);
				size_t colon_pos = location.rfind(":");
				if (colon_pos != std::string::npos) {
					current_test_file = location.substr(0, colon_pos);
					try {
						failure_line = std::stoi(location.substr(colon_pos + 1));
					} catch (...) {
						failure_line = -1;
					}
					if (debug) {
						fprintf(stderr, "DEBUG: Extracted file='%s' line=%d\n", current_test_file.c_str(), failure_line);
					}
				}
			}
		}

		// Capture SQL query in failure section
		else if (in_failure_section && !line.empty() &&
		         line.find("================================================================================") ==
		             std::string::npos &&
		         line.find("SELECT") == 0) {
			failure_query = line;
		}

		// Capture mismatch details
		else if (in_failure_section && line.find("Mismatch on row") != std::string::npos) {
			mismatch_details = line;
		}

		// Detect expected result section
		else if (in_failure_section && line.find("Expected result:") != std::string::npos) {
			in_expected_section = true;
			in_actual_section = false;
		}

		// Detect actual result section
		else if (in_failure_section && line.find("Actual result:") != std::string::npos) {
			in_expected_section = false;
			in_actual_section = true;
		}

		// End of failure section - create failure event
		// IMPORTANT: Check this BEFORE result data capture to avoid FAILED: line being captured as result
		else if (in_failure_section && line.find("FAILED:") != std::string::npos) {
			if (debug) {
				fprintf(stderr, "DEBUG: FAILED: found, creating event. file='%s' line=%d msg='%s'\n",
				        current_test_file.c_str(), failure_line, failure_message.c_str());
			}
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "duckdb_test";
			event.event_type = ValidationEventType::TEST_RESULT;
			event.ref_file = current_test_file;
			event.ref_line = failure_line;
			event.ref_column = -1;
			event.function_name = failure_query.empty() ? "unknown" : failure_query.substr(0, 50);
			event.status = ValidationEventStatus::FAIL;
			event.category = "test_failure";

			// Enhanced message with mismatch details
			std::string enhanced_message = failure_message;
			if (!mismatch_details.empty()) {
				enhanced_message += " | " + mismatch_details;
			}
			event.message = enhanced_message;

			// Enhanced raw_output with query and comparison details
			std::string enhanced_output = failure_query;
			if (!expected_result.empty() && !actual_result.empty()) {
				enhanced_output += "\n--- Expected ---\n" + expected_result;
				enhanced_output += "\n--- Actual ---\n" + actual_result;
			}
			event.log_content = enhanced_output;

			// Use suggestion field for mismatch details
			event.suggestion = mismatch_details;
			event.execution_time = 0.0;

			events.push_back(event);

			// Reset failure tracking
			in_failure_section = false;
			in_expected_section = false;
			in_actual_section = false;
			failure_message.clear();
			failure_query.clear();
			failure_line = -1;
			mismatch_details.clear();
			expected_result.clear();
			actual_result.clear();
		}

		// Capture expected result data
		else if (in_expected_section && !line.empty() &&
		         line.find("================================================================================") ==
		             std::string::npos) {
			if (!expected_result.empty())
				expected_result += "\n";
			expected_result += line;
		}

		// Capture actual result data
		else if (in_actual_section && !line.empty() &&
		         line.find("================================================================================") ==
		             std::string::npos) {
			if (!actual_result.empty())
				actual_result += "\n";
			actual_result += line;
		}

		// Parse summary line: test cases: X | Y passed | Z failed
		else if (line.find("test cases:") != std::string::npos) {
			// Extract summary statistics and create summary events
			std::string summary_line = line;

			// Extract passed count
			size_t passed_pos = summary_line.find(" passed");
			if (passed_pos != std::string::npos) {
				size_t passed_start = summary_line.rfind(" ", passed_pos - 1);
				if (passed_start != std::string::npos) {
					try {
						int passed_count =
						    std::stoi(summary_line.substr(passed_start + 1, passed_pos - passed_start - 1));

						// Create passed test events (summary)
						ValidationEvent summary_event;
						summary_event.event_id = event_id++;
						summary_event.tool_name = "duckdb_test";
						summary_event.event_type = ValidationEventType::TEST_RESULT;
						summary_event.status = ValidationEventStatus::INFO;
						summary_event.category = "test_summary";
						summary_event.message = "Test summary: " + std::to_string(passed_count) + " tests passed";
						summary_event.ref_line = -1;
						summary_event.ref_column = -1;
						summary_event.execution_time = 0.0;

						events.push_back(summary_event);
					} catch (...) {
						// Ignore parsing errors
					}
				}
			}
		}
	}

	// If no events were created, add a basic summary
	if (events.empty()) {
		ValidationEvent summary_event;
		summary_event.event_id = 1;
		summary_event.tool_name = "duckdb_test";
		summary_event.event_type = ValidationEventType::TEST_RESULT;
		summary_event.status = ValidationEventStatus::INFO;
		summary_event.category = "test_summary";
		summary_event.message = "DuckDB test output parsed (no specific test results found)";
		summary_event.ref_line = -1;
		summary_event.ref_column = -1;
		summary_event.execution_time = 0.0;

		events.push_back(summary_event);
	}
}

} // namespace duckdb
