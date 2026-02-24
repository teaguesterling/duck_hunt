#include "unity_test_xml_parser.hpp"
#include "parsers/base/safe_parsing.hpp"
#include "core/webbed_integration.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/query_result.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

bool UnityTestXmlParser::canParse(const std::string &content) const {
	// No LooksLikeXml gate: mixed-format content (editor logs before XML)
	// is common with Unity. HasRootElement + NUnit attributes are sufficient
	// since <test-run + testcasecount= is highly specific to NUnit 3 XML.
	if (!HasRootElement(content, "test-run")) {
		return false;
	}

	return content.find("testcasecount=") != std::string::npos || content.find("engine-version=") != std::string::npos;
}

std::vector<ValidationEvent> UnityTestXmlParser::parseWithContext(ClientContext &context,
                                                                  const std::string &content) const {
	// Framework already extracted clean XML via MaybeExtractContent.
	// Use parse_xml to parse directly from string — no temp files needed.
	if (!WebbedIntegration::TryAutoLoadWebbed(context)) {
		throw InvalidInputException(WebbedIntegration::GetWebbedRequiredError());
	}

	auto result = WebbedIntegration::ParseXml(context, content, "test-case");


	std::vector<ValidationEvent> events;
	int64_t event_id = 1;

	// Get column indices for the fields we need
	auto &names = result->names;

	idx_t name_idx = DConstants::INVALID_INDEX;
	idx_t fullname_idx = DConstants::INVALID_INDEX;
	idx_t methodname_idx = DConstants::INVALID_INDEX;
	idx_t classname_idx = DConstants::INVALID_INDEX;
	idx_t result_idx = DConstants::INVALID_INDEX;
	idx_t duration_idx = DConstants::INVALID_INDEX;
	idx_t failure_idx = DConstants::INVALID_INDEX;
	idx_t reason_idx = DConstants::INVALID_INDEX;
	idx_t output_idx = DConstants::INVALID_INDEX;

	for (idx_t i = 0; i < names.size(); i++) {
		if (names[i] == "name")
			name_idx = i;
		else if (names[i] == "fullname")
			fullname_idx = i;
		else if (names[i] == "methodname")
			methodname_idx = i;
		else if (names[i] == "classname")
			classname_idx = i;
		else if (names[i] == "result")
			result_idx = i;
		else if (names[i] == "duration")
			duration_idx = i;
		else if (names[i] == "failure")
			failure_idx = i;
		else if (names[i] == "reason")
			reason_idx = i;
		else if (names[i] == "output")
			output_idx = i;
	}

	// Process each row (test case)
	while (true) {
		auto chunk = result->Fetch();
		if (!chunk || chunk->size() == 0) {
			break;
		}

		for (idx_t row = 0; row < chunk->size(); row++) {
			ValidationEvent event;
			event.event_id = event_id++;
			event.tool_name = "unity_test";
			event.event_type = ValidationEventType::TEST_RESULT;

			// Extract fields
			std::string name, fullname, methodname, classname, test_result, duration_str;

			if (name_idx != DConstants::INVALID_INDEX) {
				auto val = chunk->data[name_idx].GetValue(row);
				if (!val.IsNull())
					name = val.ToString();
			}
			if (fullname_idx != DConstants::INVALID_INDEX) {
				auto val = chunk->data[fullname_idx].GetValue(row);
				if (!val.IsNull())
					fullname = val.ToString();
			}
			if (methodname_idx != DConstants::INVALID_INDEX) {
				auto val = chunk->data[methodname_idx].GetValue(row);
				if (!val.IsNull())
					methodname = val.ToString();
			}
			if (classname_idx != DConstants::INVALID_INDEX) {
				auto val = chunk->data[classname_idx].GetValue(row);
				if (!val.IsNull())
					classname = val.ToString();
			}
			if (result_idx != DConstants::INVALID_INDEX) {
				auto val = chunk->data[result_idx].GetValue(row);
				if (!val.IsNull())
					test_result = val.ToString();
			}
			if (duration_idx != DConstants::INVALID_INDEX) {
				auto val = chunk->data[duration_idx].GetValue(row);
				if (!val.IsNull())
					duration_str = val.ToString();
			}

			// Set test identification
			event.test_name = !fullname.empty() ? fullname : name;
			event.function_name = !methodname.empty() ? methodname : name;
			event.ref_file = classname;

			// Parse duration
			if (!duration_str.empty()) {
				try {
					event.execution_time = SafeParsing::SafeStod(duration_str);
				} catch (...) {
					event.execution_time = 0.0;
				}
			}

			// Determine status from result
			if (test_result == "Passed") {
				event.status = ValidationEventStatus::PASS;
				event.severity = "info";
				event.category = "test_pass";
				event.message = "Test passed";
			} else if (test_result == "Failed") {
				event.status = ValidationEventStatus::FAIL;
				event.severity = "error";
				event.category = "test_failure";
				event.message = "Test failed";

				// Try to extract failure message from failure struct
				if (failure_idx != DConstants::INVALID_INDEX) {
					auto val = chunk->data[failure_idx].GetValue(row);
					if (!val.IsNull()) {
						// failure is likely a struct with message and stack-trace
						event.log_content = val.ToString();
					}
				}
			} else if (test_result == "Skipped" || test_result == "Ignored") {
				event.status = ValidationEventStatus::SKIP;
				event.severity = "info";
				event.category = "test_skipped";
				event.message = "Test skipped";

				// Try to extract skip reason
				if (reason_idx != DConstants::INVALID_INDEX) {
					auto val = chunk->data[reason_idx].GetValue(row);
					if (!val.IsNull()) {
						event.message = val.ToString();
					}
				}
			} else if (test_result == "Inconclusive") {
				event.status = ValidationEventStatus::WARNING;
				event.severity = "warning";
				event.category = "test_inconclusive";
				event.message = "Test inconclusive";
			} else {
				event.status = ValidationEventStatus::WARNING;
				event.severity = "warning";
				event.category = "test_unknown";
				event.message = "Unknown test result: " + test_result;
			}

			// Extract test output if available
			if (output_idx != DConstants::INVALID_INDEX) {
				auto val = chunk->data[output_idx].GetValue(row);
				if (!val.IsNull()) {
					std::string output = val.ToString();
					if (!output.empty()) {
						if (event.log_content.empty()) {
							event.log_content = output;
						} else {
							event.log_content += "\n\n--- Test Output ---\n" + output;
						}
					}
				}
			}

			event.structured_data = "unity_test_xml";
			events.push_back(std::move(event));
		}
	}

	return events;
}

} // namespace duckdb
